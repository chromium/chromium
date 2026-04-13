// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_model_executor.h"

#include <algorithm>

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

namespace contextual_tasks {

ContextualTasksContextModelExecutor::ContextualTasksContextModelExecutor() = default;
ContextualTasksContextModelExecutor::~ContextualTasksContextModelExecutor() = default;

bool ContextualTasksContextModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  if (input_tensors.empty() || !input_tensors[0]) {
    return false;
  }

  return tflite::task::core::PopulateTensor<float>(input, input_tensors[0]).ok();
}

std::optional<float> ContextualTasksContextModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  if (output_tensors.empty() || !output_tensors[0]) {
    return std::nullopt;
  }

  // The model outputs a single relevance score.
  const TfLiteTensor* tensor = output_tensors[0];
  std::vector<float> output;
  if (!tflite::task::core::PopulateVector<float>(tensor, &output).ok() ||
      output.empty()) {
    return std::nullopt;
  }

  return output[0];
}

}  // namespace contextual_tasks
