// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_EXECUTOR_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_EXECUTOR_H_

#include <vector>

#include "components/optimization_guide/core/base_model_executor.h"

// A model executor to run the history clusters module ranking model.
class HistoryClustersModuleRankingModelExecutor
    : public optimization_guide::BaseModelExecutor<float,
                                                   const std::vector<float>&> {
 public:
  HistoryClustersModuleRankingModelExecutor();
  ~HistoryClustersModuleRankingModelExecutor() override;

  HistoryClustersModuleRankingModelExecutor(
      const HistoryClustersModuleRankingModelExecutor&) = delete;
  HistoryClustersModuleRankingModelExecutor& operator=(
      const HistoryClustersModuleRankingModelExecutor&) = delete;

 protected:
  // optimization_guide::BaseModelExecutor:
  bool Preprocess(const std::vector<TfLiteTensor*>& input_tensors,
                  const std::vector<float>& input) override;
  absl::optional<float> Postprocess(
      const std::vector<const TfLiteTensor*>& output_tensors) override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_EXECUTOR_H_
