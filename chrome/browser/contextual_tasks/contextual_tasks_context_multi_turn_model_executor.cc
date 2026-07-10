// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_executor.h"

#include <string>

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace contextual_tasks {

ContextualTasksContextMultiTurnModelExecutor::
    ContextualTasksContextMultiTurnModelExecutor() = default;
ContextualTasksContextMultiTurnModelExecutor::
    ~ContextualTasksContextMultiTurnModelExecutor() = default;

bool ContextualTasksContextMultiTurnModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const MultiTurnModelInput& input) {
  for (TfLiteTensor* tensor : input_tensors) {
    if (!tensor) {
      continue;
    }
    std::string name(tensor->name);
    const std::vector<float>* data = nullptr;
    if (name.find("conversation_history_query_embeddings") != std::string::npos) {
      data = &input.conversation_thread_queries_embeddings;
    } else if (name.find("conversation_history_title_embeddings") != std::string::npos) {
      data = &input.conversation_thread_titles_embeddings;
    } else if (name.find("active_passages_embeddings") != std::string::npos) {
      data = &input.active_passages_embeddings;
    } else if (name.find("candidate_passages_embeddings") != std::string::npos) {
      data = &input.candidate_tab_passages_embeddings;
    } else if (name.find("active_title_embedding") != std::string::npos) {
      data = &input.active_title_embedding;
    } else if (name.find("candidate_title_embedding") != std::string::npos) {
      data = &input.candidate_tab_title_embedding;
    } else if (name.find("query_embedding") != std::string::npos) {
      data = &input.query_embedding;
    } else if (name.find("query_length") != std::string::npos) {
      data = &input.query_length;
    } else if (name.find("lexical_match_score") != std::string::npos) {
      data = &input.query_title_lexical_similarity;
    } else if (name.find("tab_recency") != std::string::npos) {
      data = &input.candidate_tab_recency;
    } else if (name.find("tab_last") != std::string::npos) {
      data = &input.candidate_tab_last_duration;
    }

    if (!data) {
      return false;
    }

    if (!tflite::task::core::PopulateTensor<float>(*data, tensor).ok()) {
      return false;
    }
  }
  return true;
}

std::optional<MultiTurnModelOutput>
ContextualTasksContextMultiTurnModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  if (output_tensors.size() < 2 || !output_tensors[0] || !output_tensors[1]) {
    return std::nullopt;
  }

  MultiTurnModelOutput output;

  std::vector<float> score_vector;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[1],
                                                 &score_vector).ok() ||
      score_vector.empty()) {
    return std::nullopt;
  }
  output.score = score_vector[0];

  if (!tflite::task::core::PopulateVector<float>(output_tensors[0],
                                                 &output.cosine_similarities).ok()) {
    return std::nullopt;
  }

  return output;
}

}  // namespace contextual_tasks
