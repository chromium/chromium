// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/passage_embeddings/passage_embeddings_coordinator.h"

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/page_content_annotations/page_content_extraction_service.h"
#include "chrome/browser/passage_embeddings/chrome_passage_embeddings_service_controller.h"
#include "components/passage_embeddings/passage_embeddings_features.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace passage_embeddings {

namespace {

std::unique_ptr<WebContentsPassageEmbedder> CreateWebContentsPassageEmbedder(
    content::WebContents* web_contents,
    WebContentsPassageEmbedder::Delegate& delegate) {
  if (kUseBackgroundPassageEmbedder.Get()) {
    return std::make_unique<WebContentsBackgroundPassageEmbedder>(web_contents,
                                                                  delegate);
  }
  return std::make_unique<WebContentsImmediatePassageEmbedder>(web_contents,
                                                               delegate);
}

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

// Provide a translation of APC to passages for the purposes of measuring
// embeddings performance. This translation is extremely simple and not intended
// to be a full fidelity representation or to be reused outside of this limited
// experiment.
std::vector<std::string> CreatePassagesFromAnnotatedPageContent(
    const optimization_guide::proto::AnnotatedPageContent&
        annotated_page_content) {
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

  const int max_words_per_aggregate_passage = kMinWordsPerPassage.Get();
  const int min_words_per_passage = kMinWordsPerPassage.Get();
  const int max_passages_per_page = kMaxPassagesPerPage.Get();

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

PassageEmbeddingsCoordinator::PassageEmbeddingsCoordinator(
    page_content_annotations::PageContentExtractionService*
        page_content_extraction_service)
    : omnibox_focus_changed_listener_(base::BindRepeating(
          &PassageEmbeddingsCoordinator::OnOmniboxFocusChanged,
          base::Unretained(this))) {
  page_content_extraction_observation_.Observe(page_content_extraction_service);
}

PassageEmbeddingsCoordinator::~PassageEmbeddingsCoordinator() = default;

void PassageEmbeddingsCoordinator::OnPageContentExtracted(
    content::Page& page,
    const optimization_guide::proto::AnnotatedPageContent& page_content) {
  std::vector<std::string> passages =
      CreatePassagesFromAnnotatedPageContent(page_content);
  VLOG(2) << "Received page content for url "
          << page_content.main_frame_data().url() << ". Generated "
          << passages.size() << " passages.";
  auto* const web_contents =
      content::WebContents::FromRenderFrameHost(&page.GetMainDocument());
  auto loc = web_contents_passage_embedders_.find(web_contents);
  if (loc == web_contents_passage_embedders_.end()) {
    loc = web_contents_passage_embedders_
              .emplace(web_contents,
                       CreateWebContentsPassageEmbedder(web_contents, *this))
              .first;
  }
  loc->second->AcceptPassages(std::move(passages));
}

Embedder::TaskId PassageEmbeddingsCoordinator::ComputePassagesEmbeddings(
    std::vector<std::string> passages,
    Embedder::ComputePassagesEmbeddingsCallback callback) {
  return ChromePassageEmbeddingsServiceController::Get()
      ->GetEmbedder()
      ->ComputePassagesEmbeddings(current_priority_, std::move(passages),
                                  std::move(callback));
}

bool PassageEmbeddingsCoordinator::TryCancel(Embedder::TaskId task_id) {
  return ChromePassageEmbeddingsServiceController::Get()
      ->GetEmbedder()
      ->TryCancel(task_id);
}

void PassageEmbeddingsCoordinator::OnWebContentsDestroyed(
    content::WebContents* web_contents) {
  web_contents_passage_embedders_.erase(web_contents);
}

void PassageEmbeddingsCoordinator::OnOmniboxFocusChanged(bool is_focused) {
  current_priority_ = is_focused ? kUrgent : kPassive;

  std::set<Embedder::TaskId> task_ids;
  for (const auto& [web_contents, web_contents_passage_embedder] :
       web_contents_passage_embedders_) {
    if (current_priority_ == kUrgent) {
      web_contents_passage_embedder
          ->MaybeProcessPendingPassagesOnPriorityIncrease();
    }

    std::optional<Embedder::TaskId> task_id =
        web_contents_passage_embedder->current_task_id();
    if (task_id) {
      task_ids.insert(*task_id);
    }
  }

  ChromePassageEmbeddingsServiceController::Get()
      ->GetEmbedder()
      ->ReprioritizeTasks(current_priority_, task_ids);
}

}  // namespace passage_embeddings
