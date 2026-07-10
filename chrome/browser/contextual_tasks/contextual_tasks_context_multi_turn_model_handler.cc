// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_handler.h"

#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_executor.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/tab_relevance_model_metadata.pb.h"

namespace contextual_tasks {
namespace {

void AppendEmbedding(const std::vector<float>& embedding,
                     int32_t num_embedding_dimensions,
                     std::vector<float>& features) {
  for (int i = 0; i < num_embedding_dimensions; ++i) {
    if (static_cast<size_t>(i) < embedding.size()) {
      features.push_back(embedding[i]);
    } else {
      features.push_back(0.0f);
    }
  }
}

void AppendMultipleEmbeddings(const std::vector<std::vector<float>>& embeddings,
                              int32_t num_embeddings_expected,
                              int32_t num_embedding_dimensions,
                              std::vector<float>& features) {
  for (int i = 0; i < num_embeddings_expected; ++i) {
    if (static_cast<size_t>(i) < embeddings.size()) {
      AppendEmbedding(embeddings[i], num_embedding_dimensions, features);
    } else {
      AppendEmbedding({}, num_embedding_dimensions, features);
    }
  }
}

}  // namespace

ContextualTasksContextMultiTurnModelHandler::
    ContextualTasksContextMultiTurnModelHandler(
        optimization_guide::OptimizationGuideModelProvider* model_provider,
        scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : optimization_guide::ModelHandler<MultiTurnModelOutput,
                                       const MultiTurnModelInput&>(
          model_provider,
          background_task_runner,
          std::make_unique<ContextualTasksContextMultiTurnModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_guide::proto::
              OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_MULTI_TURN_TAB_RELEVANCE,
          /*model_metadata=*/std::nullopt) {
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(true);
}

ContextualTasksContextMultiTurnModelHandler::
    ~ContextualTasksContextMultiTurnModelHandler() = default;

void ContextualTasksContextMultiTurnModelHandler::
    BatchExecuteModelWithSignalsForConversationThread(
        const QueryStateSignals& query_signals,
        const std::vector<TabSignals>& batch_tab_signals,
        base::OnceCallback<
            void(const std::vector<std::optional<MultiTurnModelOutput>>&)>
                callback) {
  std::optional<optimization_guide::proto::TabRelevanceModelMetadata> metadata =
      ParsedSupportedFeaturesForLoadedModel<
          optimization_guide::proto::TabRelevanceModelMetadata>();
  if (!metadata) {
    std::move(callback).Run(std::vector<std::optional<MultiTurnModelOutput>>(
        batch_tab_signals.size()));
    return;
  }

  std::vector<MultiTurnModelInput> ml_inputs;
  ml_inputs.reserve(batch_tab_signals.size());
  for (const auto& tab_signals : batch_tab_signals) {
    MultiTurnModelInput input;

    AppendEmbedding(query_signals.query_embedding,
                    metadata->num_embedding_dimensions(),
                    input.query_embedding);

    AppendMultipleEmbeddings(
        query_signals.conversation_thread_queries_embeddings,
        metadata->num_conversation_thread_turns(),
        metadata->num_embedding_dimensions(),
        input.conversation_thread_queries_embeddings);

    AppendMultipleEmbeddings(
        query_signals.conversation_thread_titles_embeddings,
        metadata->max_titles_per_thread(),
        metadata->num_embedding_dimensions(),
        input.conversation_thread_titles_embeddings);

    AppendEmbedding(query_signals.context_tab_title_embedding,
                    metadata->num_embedding_dimensions(),
                    input.active_title_embedding);

    AppendMultipleEmbeddings(query_signals.context_tab_passages_embeddings,
                             metadata->num_passages_per_tab(),
                             metadata->num_embedding_dimensions(),
                             input.active_passages_embeddings);

    AppendEmbedding(tab_signals.candidate_title_embedding,
                    metadata->num_embedding_dimensions(),
                    input.candidate_tab_title_embedding);

    AppendMultipleEmbeddings(tab_signals.candidate_passages_embeddings,
                             metadata->num_passages_per_tab(),
                             metadata->num_embedding_dimensions(),
                             input.candidate_tab_passages_embeddings);

    input.query_length.push_back(
        static_cast<float>(query_signals.query_word_count));

    input.query_title_lexical_similarity.push_back(
        static_cast<float>(tab_signals.num_query_title_matching_words));

    input.candidate_tab_recency.push_back(
        tab_signals.duration_since_last_active.has_value()
            ? tab_signals.duration_since_last_active->InSecondsF()
            : -1.0f);

    input.candidate_tab_last_duration.push_back(
        tab_signals.duration_of_last_visit.has_value()
            ? tab_signals.duration_of_last_visit->InSecondsF()
            : -1.0f);

    ml_inputs.push_back(std::move(input));
  }

  BatchExecuteModelWithInput(std::move(callback), ml_inputs);
}

}  // namespace contextual_tasks
