// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_HANDLER_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_HANDLER_H_

#include <optional>
#include <vector>

#include "base/functional/callback.h"
#include "base/gtest_prod_util.h"
#include "chrome/browser/contextual_tasks/contextual_tasks_context_multi_turn_model_executor.h"
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

// Handler for the Contextual Tasks Multi-Turn Tab Relevance model.
class ContextualTasksContextMultiTurnModelHandler
    : public optimization_guide::ModelHandler<MultiTurnModelOutput,
                                              const MultiTurnModelInput&> {
 public:
  ContextualTasksContextMultiTurnModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      scoped_refptr<base::SequencedTaskRunner> background_task_runner);
  ~ContextualTasksContextMultiTurnModelHandler() override;

  // Executes the model asynchronously for a batch of tabs.
  void BatchExecuteModelWithSignalsForConversationThread(
      const QueryStateSignals& query_signals,
      const std::vector<TabSignals>& batch_tab_signals,
      base::OnceCallback<
          void(const std::vector<std::optional<MultiTurnModelOutput>>&)>
              callback);

};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MULTI_TURN_MODEL_HANDLER_H_
