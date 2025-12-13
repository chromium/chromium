// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/embeddings_candidate_generator.h"

#include "base/strings/strcat.h"
#include "components/passage_embeddings/passage_embeddings_features.h"

namespace passage_embeddings {

namespace {

void CollectTextForContentNode(
    const optimization_guide::proto::ContentNode& node,
    std::vector<std::string>& text) {
  if (!node.has_content_attributes()) {
    return;
  }

  const auto& attributes = node.content_attributes();

  switch (attributes.attribute_type()) {
    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_TABLE:
      if (!attributes.table_data().table_name().empty()) {
        text.push_back(attributes.table_data().table_name());
      }
      break;

    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_TEXT:
      if (!attributes.text_data().text_content().empty()) {
        text.push_back(attributes.text_data().text_content());
      }
      break;

    case optimization_guide::proto::ContentAttributeType::
        CONTENT_ATTRIBUTE_IMAGE:
      if (!attributes.image_data().image_caption().empty()) {
        text.push_back(attributes.image_data().image_caption());
      }
      break;

    default:
      break;
  }
}

void CollectTextForContentNodesRecursively(
    const optimization_guide::proto::ContentNode& node,
    std::vector<std::string>& text) {
  CollectTextForContentNode(node, text);

  for (const auto& child : node.children_nodes()) {
    CollectTextForContentNodesRecursively(child, text);
  }
}

int CountWords(std::string_view s) {
  if (s.empty()) {
    return 0;
  }
  int word_count = (s[0] == ' ') ? 0 : 1;
  for (size_t i = 1; i < s.length(); i++) {
    if (s[i] != ' ' && s[i - 1] == ' ') {
      word_count++;
    }
  }
  return word_count;
}

// Provide a translation of APC to passages. This translation is extremely
// simple and not intended to be a full fidelity representation.
std::vector<std::string> CreatePassagesFromAnnotatedPageContent(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content,
    int max_passages_per_page) {
  std::vector<std::string> text;
  CollectTextForContentNodesRecursively(annotated_page_content.root_node(),
                                        text);

  if (text.empty()) {
    return {};
  }

  const auto append_with_whitespace_separator =
      [](std::string& str, std::string_view str_to_append) {
        if (str_to_append.empty()) {
          return;
        }

        if (str.empty() || str.back() == ' ') {
          str.append(str_to_append);
          return;
        }

        base::StrAppend(&str, {" ", str_to_append});
      };

  const int max_words_per_aggregate_passage =
      kMaxWordsPerAggregatePassage.Get();
  const int min_words_per_passage = kMinWordsPerPassage.Get();

  std::vector<std::string> passages;
  passages.push_back("");
  int current_passage_words = 0;

  for (const std::string& item : text) {
    const int item_words = CountWords(item);

    if (current_passage_words >= max_words_per_aggregate_passage) {
      if (passages.size() >= static_cast<size_t>(max_passages_per_page)) {
        break;
      }
      passages.push_back("");
      current_passage_words = 0;
    }

    const bool should_append =
        current_passage_words < min_words_per_passage ||
        current_passage_words + item_words <= max_words_per_aggregate_passage;

    if (should_append) {
      append_with_whitespace_separator(passages.back(), item);
      current_passage_words += item_words;
    }
  }

  if (passages.back() == "") {
    passages.pop_back();
  }

  return passages;
}

}  // namespace

std::vector<std::pair<std::string, PassageType>> GenerateEmbeddingsCandidates(
    const optimization_guide::proto::AnnotatedPageContent& apc,
    int page_content_passages_to_generate) {
  std::vector<std::pair<std::string, PassageType>> candidates;

  // Push back passage candidates.
  std::vector<std::string> passages = CreatePassagesFromAnnotatedPageContent(
      apc, page_content_passages_to_generate);
  for (const auto& passage : passages) {
    candidates.emplace_back(passage, PassageType::kPageContent);
  }

  // Push back title candidate.
  candidates.emplace_back(apc.main_frame_data().title(), PassageType::kTitle);

  return candidates;
}

}  // namespace passage_embeddings
