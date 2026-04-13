// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_handler.h"

#include <vector>

#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_executor.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"
#include "components/optimization_guide/core/delivery/optimization_guide_model_provider.h"
#include "components/optimization_guide/proto/models.pb.h"
#include "components/optimization_guide/proto/tab_relevance_model_metadata.pb.h"

namespace contextual_tasks {

namespace {

void AppendPassageSimilarities(const std::vector<ScoredPassage>& similarities,
                               int32_t num_passages_per_tab,
                               std::vector<float>& features) {
  for (int i = 0; i < num_passages_per_tab; ++i) {
    if (static_cast<size_t>(i) < similarities.size()) {
      features.push_back(similarities[i].score);
    } else {
      features.push_back(0.0f);
    }
  }
}

}  // namespace

ContextualTasksContextModelHandler::ContextualTasksContextModelHandler(
    optimization_guide::OptimizationGuideModelProvider* model_provider,
    scoped_refptr<base::SequencedTaskRunner> background_task_runner)
    : optimization_guide::ModelHandler<float, const std::vector<float>&>(
          model_provider,
          background_task_runner,
          std::make_unique<ContextualTasksContextModelExecutor>(),
          /*model_inference_timeout=*/std::nullopt,
          optimization_guide::proto::
              OPTIMIZATION_TARGET_CONTEXTUAL_TASKS_TAB_RELEVANCE,
          /*model_metadata=*/std::nullopt) {
  SetShouldPreloadModel(true);
  SetShouldUnloadModelOnComplete(true);
}

ContextualTasksContextModelHandler::~ContextualTasksContextModelHandler() =
    default;

double ContextualTasksContextModelHandler::ExecuteModelWithSignalsSync(
    const QueryStateSignals& query_signals,
    const TabSignals& tab_signals) {
  std::optional<optimization_guide::proto::TabRelevanceModelMetadata> metadata =
      ParsedSupportedFeaturesForLoadedModel<
          optimization_guide::proto::TabRelevanceModelMetadata>();
  if (!metadata) {
    return 0.0;
  }

  std::vector<float> ml_features =
      ExtractModelFeatures(*metadata, query_signals, tab_signals);
  if (ml_features.empty()) {
    return 0.0;
  }

  // TODO(b/462793437): Batch all tabs and execute model once in a follow-up CL.
  std::vector<std::optional<float>> batch_outputs =
      BatchExecuteModelWithInputSync({ml_features});

  return batch_outputs.empty() || !batch_outputs[0].has_value()
             ? 0.0
             : *batch_outputs[0];
}

// static
std::vector<float> ContextualTasksContextModelHandler::ExtractModelFeatures(
    const optimization_guide::proto::TabRelevanceModelMetadata& metadata,
    const QueryStateSignals& query_signals,
    const TabSignals& tab_signals) {
  std::vector<float> features;
  features.reserve(metadata.num_features());

  for (int feature : metadata.feature_sequence()) {
    switch (feature) {
      case optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_LENGTH:
        features.push_back(query_signals.query_word_count);
        break;
      case optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_TITLE_LEXICAL_SIMILARITY:
        features.push_back(tab_signals.num_query_title_matching_words);
        break;
      case optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_ACTIVE_TAB_SIMILARITY:
        features.push_back(query_signals.query_active_tab_title_similarity);
        AppendPassageSimilarities(
            query_signals.query_active_tab_passage_similarities,
            metadata.num_passages_per_tab(), features);
        break;
      case optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_QUERY_CANDIDATE_TAB_SIMILARITY:
        features.push_back(tab_signals.query_candidate_tab_title_similarity);
        AppendPassageSimilarities(
            tab_signals.query_candidate_tab_passage_similarities,
            metadata.num_passages_per_tab(), features);
        break;
      case optimization_guide::proto::TabRelevanceModelMetadata::
          TAB_RELEVANCE_FEATURE_ACTIVE_CANDIDATE_TAB_SIMILARITY:
        features.push_back(tab_signals.active_title_candidate_title_similarity);
        break;
      default:
        // Padded for unknown features to maintain vector size.
        features.push_back(0.0f);
        break;
    }
  }

  return features;
}

}  // namespace contextual_tasks
