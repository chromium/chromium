// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_model_executor.h"

#include "third_party/tflite_support/src/tensorflow_lite_support/cc/task/core/task_utils.h"

HistoryClustersModuleRankingModelExecutor::
    HistoryClustersModuleRankingModelExecutor() = default;
HistoryClustersModuleRankingModelExecutor::
    ~HistoryClustersModuleRankingModelExecutor() = default;

bool HistoryClustersModuleRankingModelExecutor::Preprocess(
    const std::vector<TfLiteTensor*>& input_tensors,
    const std::vector<float>& input) {
  CHECK_EQ(input.size(), input_tensors.size());
  CHECK_EQ(kTfLiteFloat32, input_tensors[0]->type);
  for (size_t i = 0; i < input.size(); ++i) {
    std::vector<float> data = {input[i]};
    absl::Status status =
        tflite::task::core::PopulateTensor<float>(data, input_tensors[i]);
    if (!status.ok()) {
      return false;
    }
  }
  return true;
}

absl::optional<float> HistoryClustersModuleRankingModelExecutor::Postprocess(
    const std::vector<const TfLiteTensor*>& output_tensors) {
  std::vector<float> output;
  if (!tflite::task::core::PopulateVector<float>(output_tensors[0], &output)
           .ok()) {
    return absl::nullopt;
  }

  CHECK_EQ(1u, output.size());
  return output[0];
}
