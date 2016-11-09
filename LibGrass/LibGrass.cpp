// LibGrass.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"
#include <fstream>
#include <sstream>
#include <iomanip>
#include "LibGrass.h"
#include "common\parser\implementations\Segment\seg_run.h"
#include "common\parser\implementations\POSTagging\postag_run.h"
#include "common\parser\implementations\arceager\arceager_depparser.h"
#include "common\parser\implementations\graph_transition\titov\titov_run.h"

Segment::Run* segmentor = nullptr;
POSTagging::Run* postagger = nullptr;
arceager::DepParser * syntax_parser = nullptr;
arceager::DepParser * psdtr_parser = nullptr;
titov::DepParser<PackedScoreType> * semantic_parser = nullptr;

std::string toHalfWidth(const std::string & input) {
	std::string temp;
	for (size_t i = 0; i < input.size(); i++) {
		if (((input[i] & 0xF0) ^ 0xE0) == 0) {
			int old_char = (input[i] & 0xF) << 12 | ((input[i + 1] & 0x3F) << 6 | (input[i + 2] & 0x3F));
			if (old_char == 0x3000) { // blank
				char new_char = 0x20;
				temp += new_char;
			}
			else if (old_char >= 0xFF01 && old_char <= 0xFF5E) { // full char
				char new_char = old_char - 0xFEE0;
				if (new_char == ',' || new_char == '?' || new_char == '!') {
					temp += input[i];
					temp += input[i + 1];
					temp += input[i + 2];
				}
				else temp += new_char;
			}
			else { // other 3 bytes char
				temp += input[i];
				temp += input[i + 1];
				temp += input[i + 2];
			}
			i = i + 2;
		}
		else {
			temp += input[i];
		}
	}
	return temp;
}

LIBGRASS_API void create_segmentor(const std::string & feature_file, const std::string & dict_file)
{
	if (segmentor == nullptr) {
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(NULL);

		segmentor = new Segment::Run();
		segmentor->initParser(feature_file, feature_file, dict_file, true);
		std::cout << "segmentor created" << std::endl;
	}
}

LIBGRASS_API void delete_segmentor()
{
	delete segmentor;
	segmentor = nullptr;
}

//����Ϊһ�仰һ��
LIBGRASS_API void seg_file(const std::string & input_file, const std::string & output_file, int encoding) {
	if (segmentor != nullptr) {
		segmentor->parse(input_file, output_file, encoding);
	}
}

//�ִ�ѵ������Ϊһ��һ���ʣ�ÿ������֮���һ��
LIBGRASS_API void train_segmentor(const std::string & train_file, const std::string & feature_file, const std::string & dict_file, int times, int encoding) {
	if (segmentor == nullptr) {
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(NULL);

		segmentor = new Segment::Run();
		segmentor->initParser(feature_file, feature_file, dict_file, false);
	}
	for (int i = 0; i < times; i++)
		segmentor->train(train_file);
}

LIBGRASS_API std::vector<std::string> seg_string(const std::string & input, int encoding) {
	if (segmentor != nullptr) {
		std::vector<std::string> result;
		std::string sentence = segmentor->parse(input, encoding), word;
		std::stringstream ss(sentence);
		while (ss >> word) result.push_back(word);
		return result;
	}
	return{};
}

LIBGRASS_API void create_postagger(const std::string & feature_file)
{
	if (postagger == nullptr) {
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(NULL);

		postagger = new POSTagging::Run();
		postagger->initParser(feature_file, feature_file, "", true);
		std::cout << "postagger created" << std::endl;
	}
}

LIBGRASS_API void delete_postagger()
{
	delete postagger;
	postagger = nullptr;
}

//����Ϊһ�仰һ�У�����֮��ո�
LIBGRASS_API void tag_file(const std::string & input_file, const std::string & output_file, int encoding) {
	if (postagger != nullptr) {
		postagger->parse(input_file, output_file, encoding);
	}
}

//���Ա�עѵ������Ϊһ��һ���ʺͶ�Ӧ���ԣ�ÿ������֮���һ��
LIBGRASS_API void train_postagger(const std::string & train_file, const std::string & feature_file, int times, int encoding)
{
	if (postagger == nullptr) {
		std::ios_base::sync_with_stdio(false);
		std::cin.tie(NULL);

		postagger = new POSTagging::Run();
		postagger->initParser(feature_file, feature_file, "", false);
	}
	for (int i = 0; i < times; i++)
		postagger->train(train_file);
}

LIBGRASS_API std::vector<std::pair<std::string, std::string>> tag_sentence(const std::vector<std::string> & input, int encoding) {
	if (postagger != nullptr) {
		return postagger->parse(input, encoding);
	}
	return{};
}

LIBGRASS_API void train_syntax_parser(const std::string & input_file, const std::string & feature_file, int round)
{
	std::cout << std::fixed << std::setprecision(4);

	DependencyTree ref_sent;

	std::ifstream input(input_file);
	syntax_parser->m_tLabels.add("ROOT");
	if (input) {
		while (input >> ref_sent) {
			for (const auto & token : ref_sent) {
				syntax_parser->m_tLabels.add(TREENODE_LABEL(token));
			}
		}
	}
	input.close();

	syntax_parser->m_AC.AL_FIRST = arceager::POP_ROOT + 1;
	syntax_parser->m_AC.AL_END = syntax_parser->m_AC.AR_FIRST = syntax_parser->m_AC.AL_FIRST + syntax_parser->m_tLabels.count();
	syntax_parser->m_AC.AR_END = syntax_parser->m_AC.AR_FIRST + syntax_parser->m_tLabels.count();

	for (int i = 1; i <= round; ++i)
	{
		int nRound = 0;
		arceager::DepParser* sp = new arceager::DepParser(feature_file, feature_file, ParserState::TRAIN);
		std::cout << "training round " << i << " start" << std::endl;
		input.open(input_file);
		if (input) {
			while (input >> ref_sent) {
				for (auto && node : ref_sent)
				{
					TREENODE_WORD(node) = toHalfWidth(TREENODE_WORD(node));
					TREENODE_POSTAG(node) = toHalfWidth(TREENODE_POSTAG(node));
				}
				++nRound;
				sp->train(ref_sent, nRound);
			}
			sp->finishtraining();
		}
		input.close();
		delete sp;
		std::cout << "training round " << i << " end" << std::endl;
	}

}

LIBGRASS_API void create_syntax_parser(const std::string & feature_file)
{
	if (syntax_parser != nullptr) delete syntax_parser;
	syntax_parser = new arceager::DepParser(feature_file, feature_file, ParserState::PARSE);
	
	syntax_parser->m_AC.AL_FIRST = arceager::POP_ROOT + 1;
	syntax_parser->m_AC.AL_END = syntax_parser->m_AC.AR_FIRST = syntax_parser->m_AC.AL_FIRST + syntax_parser->m_tLabels.count();
	syntax_parser->m_AC.AR_END = syntax_parser->m_AC.AR_FIRST + syntax_parser->m_tLabels.count();

	std::cout << "syntax parser created" << std::endl;
}

LIBGRASS_API void delete_syntax_parser()
{
	if (syntax_parser != nullptr) delete syntax_parser;
}

LIBGRASS_API void syntax_parse_file(const std::string & input_file, const std::string & output_file, int encoding)
{
	Sentence sentence;
	DependencyTree tree;

	std::ifstream input(input_file);
	std::ofstream output(output_file);

	if (input) {
		while (input >> sentence) {
			for (auto && node : sentence)
			{
				SENT_WORD(node) = toHalfWidth(SENT_WORD(node));
				SENT_POSTAG(node) = toHalfWidth(SENT_POSTAG(node));
			}
			if (sentence.size() < MAX_SENTENCE_SIZE) {
				syntax_parser->parse(sentence, &tree);
				output << tree;
				tree.clear();
			}
		}
		std::cout << std::endl;
	}
	input.close();
	output.close();
}

LIBGRASS_API std::string syntax_parse_string(const std::string & input, int encoding)
{
	std::string output;
	Sentence sentence;
	DependencyTree tree;

	std::stringstream ss(input);
	ss >> sentence;
	if (sentence.size() < MAX_SENTENCE_SIZE) {
		for (auto && node : sentence)
		{
			SENT_WORD(node) = toHalfWidth(SENT_WORD(node));
			SENT_POSTAG(node) = toHalfWidth(SENT_POSTAG(node));
		}
		syntax_parser->parse(sentence, &tree, false);
		ss.clear();
		ss << tree;
		ss >> output;
	}

	return output;
}

void format_semantic_input(const std::string & input_file, const std::string & output_file)
{
	std::ifstream input(input_file);
	std::ofstream output(output_file);
	Sentence sentence;
	DependencyGraph graph;
	while (input >> sentence)
	{
		DependencyGraphNode root;
		root.m_sWord = "#root#";
		root.m_sPOSTag = "#ROOT#";
		root.m_nTreeHead = -1;
		root.m_sSuperTag = "_";
		graph.add(root);
		for (const auto & token : sentence)
		{
			DependencyGraphNode node;
			node.m_sWord = SENT_WORD(token);
			node.m_sPOSTag = SENT_POSTAG(token);
			node.m_nTreeHead = 0;
			node.m_sSuperTag = "_";
			graph.add(node);
		}
		output << graph;
		graph.clear();
	}
	input.close();
	output.close();
}

LIBGRASS_API void train_semantic_parser(const std::string & input_file, const std::string & feature_file, int round)
{
	std::cout << std::fixed << std::setprecision(4);
	for (int i = 0; i < round; ++i)
	{
		titov::DepParser<PackedScoreType> * sp = new titov::DepParser<PackedScoreType>(input_file, feature_file, feature_file, ParserState::TRAIN, true, true, false);

		int nRound = 0;
		DependencyGraph sentence;

		std::ifstream input(input_file);
		if (input) {
			while (input >> sentence) {
				sp->train(sentence, ++nRound);
			}
			sp->finishtraining();
		}
		input.close();

		delete sp;
	}
}

LIBGRASS_API void create_semantic_parser(const std::string & semantic_feature_file, const std::string & tree_feature_file)
{
	if (semantic_parser != nullptr) delete semantic_parser;
	semantic_parser = new titov::DepParser<PackedScoreType>("", semantic_feature_file, semantic_feature_file, ParserState::PARSE, true, true, false);

	if (psdtr_parser != nullptr) delete psdtr_parser;
	psdtr_parser = new arceager::DepParser(tree_feature_file, tree_feature_file, ParserState::PARSE);

	psdtr_parser->m_AC.AL_FIRST = arceager::POP_ROOT + 1;
	psdtr_parser->m_AC.AL_END = psdtr_parser->m_AC.AR_FIRST = psdtr_parser->m_AC.AL_FIRST + psdtr_parser->m_tLabels.count();
	psdtr_parser->m_AC.AR_END = psdtr_parser->m_AC.AR_FIRST + psdtr_parser->m_tLabels.count();

	std::cout << "sematic parser created" << std::endl;
}

LIBGRASS_API void delete_semantic_parser()
{
	if (semantic_parser != nullptr) delete semantic_parser;
	if (psdtr_parser != nullptr) delete psdtr_parser;
}

LIBGRASS_API void semantic_parse_file(const std::string & input_file, const std::string & output_file, int encoding)
{
	DependencyGraph sentence;
	DependencyGraph graph;

	format_semantic_input(input_file, input_file + ".graph");
	std::ifstream input(input_file + ".graph");
	std::ofstream output(output_file);

	if (input) {
		while (input >> sentence) {
			Sentence sent;
			DependencyTree tree;
			for (auto && node : sentence)
			{
				node.m_sWord = toHalfWidth(node.m_sWord);
				node.m_sPOSTag = toHalfWidth(node.m_sPOSTag);
				sent.push_back(POSTaggedWord(node.m_sWord, node.m_sPOSTag));
			}
			if (sentence.size() < MAX_SENTENCE_SIZE) {
				psdtr_parser->parse(sent, &tree, false);
				for (int i = 0; i < tree.size(); ++i) {
					sentence[i].m_nTreeHead = TREENODE_HEAD(tree[i]);
				}
				semantic_parser->parse(sentence, &graph);
				output << graph;
				graph.clear();
			}
		}
		std::cout << std::endl;
	}
	input.close();
	output.close();
	remove((input_file + ".graph").c_str());
}

LIBGRASS_API std::string semantic_parse_string(const std::string & input, int encoding)
{
	DependencyGraph sentence;
	DependencyGraph graph;
	Sentence sent;
	DependencyTree tree;

	std::string output;
	std::stringstream ss(input);
	ss >> sent;

	for (auto && node : sent)
	{
		SENT_WORD(node) = toHalfWidth(SENT_WORD(node));
		SENT_POSTAG(node) = toHalfWidth(SENT_POSTAG(node));
	}

	DependencyGraphNode root;
	root.m_sWord = "#root#";
	root.m_sPOSTag = "#ROOT#";
	root.m_nTreeHead = -1;
	root.m_sSuperTag = "_";
	sentence.add(root);
	for (const auto & token : sent)
	{
		DependencyGraphNode node;
		node.m_sWord = SENT_WORD(token);
		node.m_sPOSTag = SENT_POSTAG(token);
		node.m_nTreeHead = 0;
		node.m_sSuperTag = "_";
		sentence.add(node);
	}

	if (sent.size() < MAX_SENTENCE_SIZE) {
		psdtr_parser->parse(sent, &tree, false);
		for (int i = 0; i < tree.size(); ++i) {
			sentence[i].m_nTreeHead = TREENODE_HEAD(tree[i]);
		}
		semantic_parser->parse(sentence, &graph);
		std::cout << std::endl;
		ss.clear();
		ss << graph;
		ss >> output;
	}

	return output;
}

LIBGRASS_API void sentence_per_line(const std::string & input_file, const std::string & output_file, int encoding) {

	std::string period = GBK2UTF8("��");
	std::string question = GBK2UTF8("��");
	std::string exclamation = GBK2UTF8("��");

	std::ifstream input(input_file);
	std::ofstream output(output_file);

	if (input) {
		std::string line, result;
		int INF = 0x3fffffff;
		int pos1 = -1, pos2 = -1, postmp = -1, size1 = 0;
		while (std::getline(input, line)) {
			int pos = 0;
			while (pos < line.length() && isspace(line[pos])) ++pos;
			if (pos < line.length()) line = line.substr(pos);
			else continue;
			std::stringstream ss(line);
			while (ss >> result) {
				if (result.find(period) == 0 || result.find(question) == 0 || result.find(exclamation) == 0) {
					output << result << std::endl;
				}
				else {
					output << result << ' ';
				}
			}
		}
	}
}