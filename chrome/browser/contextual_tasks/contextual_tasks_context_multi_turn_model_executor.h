// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_EXECUTOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/optimization_guide/core/inference/base_model_executor.h"

namespace contextual_tasks {

// Structured input containing pre-padded vectors for the 11 model inputs
struct MultiTurnModelInput {
  std::vector<float> query_embedding;
  std::vector<float> conversation_thread_queries_embeddings;
  std::vector<float> conversation_thread_titles_embeddings;
  std::vector<float> active_title_embedding;
  std::vector<float> active_passages_embeddings;
  std::vector<float> candidate_tab_title_embedding;
  std::vector<float> candidate_tab_passages_embeddings;
  std::vector<float> query_length;
  std::vector<float> query_title_lexical_similarity;
  std::vector<float> candidate_tab_recency;
  std::vector<float> candidate_tab_last_duration;
};

// Structured output containing the score and all similarities
struct MultiTurnModelOutput {
  float score;
  std::vector<float> cosine_similarities;
};

// Executor for the Contextual Tasks Multi-Turn Tab Relevance model.
class ContextualTasksContextMultiTurnModelExecutor
    : public optimization_guide::BaseModelExecutor<MultiTurnModelOutput,
                                                   const MultiTurnModelInput&> {
 public:
  ContextualTasksContextMultiTurnModelExecutor();
  ~ContextualTasksContextMultiTurnModelExecutor() override;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const MultiTurnModelInput& input) override;
  std::optional<MultiTurnModelOutput> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_EXECUTOR_H_
