// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/preloading_model_executor.h"

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

PreloadingModelExecutor::PreloadingModelExecutor() = default;
PreloadingModelExecutor::~PreloadingModelExecutor() = default;

bool PreloadingModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  return tflite::task::core::PopulateTensor<float>(input, input_tensors[0])
      .ok();
}

std::optional<float> PreloadingModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> output;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &output)
           .ok()) {
    return std::nullopt;
  }

  CHECK_EQ(1u, output.size());
  return output[0];
}
