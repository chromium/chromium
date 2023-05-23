// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_HANDLER_H_

#include <memory>
#include <vector>

#include "base/containers/unique_ptr_adapters.h"
#include "base/functional/callback.h"
#include "components/optimization_guide/core/model_handler.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

class HistoryClustersModuleRankingSignals;

// Model handler used to retrieve and eventually execute the model.
class HistoryClustersModuleRankingModelHandler
    : public optimization_guide::ModelHandler<float,
                                              const std::vector<float>&> {
 public:
  explicit HistoryClustersModuleRankingModelHandler(
      optimization_guide::OptimizationGuideModelProvider* model_provider);
  ~HistoryClustersModuleRankingModelHandler() override;
  HistoryClustersModuleRankingModelHandler(
      const HistoryClustersModuleRankingModelHandler&) = delete;
  HistoryClustersModuleRankingModelHandler& operator=(
      const HistoryClustersModuleRankingModelHandler&) = delete;

  // Whether the available model can be executed.
  //
  // Virtual for testing.
  virtual bool CanExecuteAvailableModel();

  using BatchModelOutput = std::vector<float>;

  // Executes model on `inputs` and invokes `callback` with the returned vector
  // being in the same order as `inputs`.
  //
  // Virtual for testing.
  using ExecuteBatchCallback = base::OnceCallback<void(BatchModelOutput)>;
  virtual void ExecuteBatch(
      std::vector<HistoryClustersModuleRankingSignals>* inputs,
      ExecuteBatchCallback callback);

 private:
  // Callback invoked when batch of inputs is complete.
  void OnBatchExecuted(BatchModelOutput* batch_job,
                       ExecuteBatchCallback callback);

  // The set of batch jobs currently pending.
  base::flat_set<std::unique_ptr<BatchModelOutput>, base::UniquePtrComparator>
      pending_jobs_;

  base::WeakPtrFactory<HistoryClustersModuleRankingModelHandler>
      weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKING_MODEL_HANDLER_H_
