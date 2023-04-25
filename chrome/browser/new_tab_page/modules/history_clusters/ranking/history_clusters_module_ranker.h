// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_

#include <memory>
#include <string>
#include <vector>

#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

class HistoryClustersModuleRankingModelHandler;

// An object that sorts a list of clusters by likelihood of re-engagement.
class HistoryClustersModuleRanker {
 public:
  HistoryClustersModuleRanker(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      const base::flat_set<std::string>& category_boostlist);
  ~HistoryClustersModuleRanker();

  // Sorts `clusters` by likelihood of re-engagement and invokes `callback` with
  // the top `max_clusters_to_return_` clusters.
  using ClustersCallback =
      base::OnceCallback<void(std::vector<history::Cluster>)>;
  void RankClusters(std::vector<history::Cluster> clusters,
                    ClustersCallback callback);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Overrides `model_handler_` with `model_handler`.
  void OverrideModelHandlerForTesting(
      std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler);
#endif

 private:
  // Runs the fallback heuristic if `model_handler_` is not instantiated or if
  // the model is not available.
  void RunFallbackHeuristic(std::vector<history::Cluster> clusters,
                            ClustersCallback callback);

  // The category boostlist to use.
  const base::flat_set<std::string> category_boostlist_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when `model_handler_` has completed scoring of `clusters`.
  void OnBatchModelExecutionComplete(std::vector<history::Cluster> clusters,
                                     ClustersCallback callback,
                                     std::vector<float> output);

  // The model handler to use for ranking clusters.
  std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler_;
#endif

  base::WeakPtrFactory<HistoryClustersModuleRanker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_
