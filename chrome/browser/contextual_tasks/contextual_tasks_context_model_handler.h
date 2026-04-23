// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_HANDLER_H_

#include <vector>

#include "base/gtest_prod_util.h"
#include "components/optimization_guide/core/inference/model_handler.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
namespace proto {
class TabRelevanceModelMetadata;
}  // namespace proto
}  // namespace optimization_guide

namespace contextual_tasks {
struct QueryStateSignals;
struct TabSignals;

// Handler for the Contextual Tasks Tab Relevance model.
class ContextualTasksContextModelHandler
    : public optimization_guide::ModelHandler<float,
                                              const std::vector<float>&> {
 public:
  ContextualTasksContextModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~ContextualTasksContextModelHandler() override;

  // Executes the model asynchronously with high-level signals.
  void ExecuteModelWithSignals(
      const QueryStateSignals& query_signals,
      const TabSignals& tab_signals,
      base::OnceCallback<void(const std::optional<float>&)> callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(ContextualTasksContextModelHandlerTest,
                           ExtractModelFeatures);

  // Extracts the features from the given signals based on the model metadata.
  static std::vector<float> ExtractModelFeatures(
      const optimization_guide::proto::TabRelevanceModelMetadata& metadata,
      const QueryStateSignals& query_signals,
      const TabSignals& tab_signals);
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_HANDLER_H_
