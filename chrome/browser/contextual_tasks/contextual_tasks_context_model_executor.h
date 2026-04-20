// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_EXECUTOR_H_
#define CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_EXECUTOR_H_

#include <optional>
#include <vector>

#include "components/optimization_guide/core/inference/base_model_executor.h"

namespace contextual_tasks {

// Executor for the Contextual Tasks Tab Relevance model.
class ContextualTasksContextModelExecutor
    : public optimization_guide::BaseModelExecutor<float,
                                                   const std::vector<float>&> {
 public:
  ContextualTasksContextModelExecutor();
  ~ContextualTasksContextModelExecutor() override;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const std::vector<float>& input) override;
  std::optional<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

}  // namespace contextual_tasks

#endif  // CHROME_BROWSER_CONTEXTUAL_TASKS_CONTEXTUAL_TASKS_CONTEXT_MODEL_EXECUTOR_H_
