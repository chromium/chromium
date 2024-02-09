// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/functional/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/cart/cart_db.h"
#include "components/history/core/browser/history_types.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/segmentation_platform/public/database_client.h"
#include "components/segmentation_platform/public/model_provider.h"

extern const char kHistoryClusterSeenEventName[];
extern const char kHistoryClusterUsedEventName[];
extern const char kHistoryClustersSeenCategoriesEventName[];
extern const char kHistoryClustersUsedCategoriesEventName[];

namespace history {
class HistoryService;
}  // namespace history

namespace optimization_guide {
class OptimizationGuideModelProvider;
}  // namespace optimization_guide

namespace segmentation_platform {
class SegmentationPlatformService;
}  // namespace segmentation_platform

class CartService;
struct HistoryClusterMetrics;
struct HistoryClustersCategoryMetrics;
class HistoryClustersModuleRankingModelHandler;
class HistoryClustersModuleRankingSignals;

// An object that sorts a list of clusters by likelihood of re-engagement.
class HistoryClustersModuleRanker {
 public:
  HistoryClustersModuleRanker(
      optimization_guide::OptimizationGuideModelProvider* model_provider,
      segmentation_platform::SegmentationPlatformService*
          segmentation_platform_service,
      history::HistoryService* history_service,
      CartService* cart_service,
      const base::flat_set<std::string>& category_boostlist);
  ~HistoryClustersModuleRanker();

  // Sorts `clusters` by likelihood of re-engagement and invokes `callback` with
  // the top `max_clusters_to_return_` clusters.
  using ClustersCallback = base::OnceCallback<void(
      std::vector<history::Cluster>,
      base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>;
  void RankClusters(std::vector<history::Cluster> clusters,
                    ClustersCallback callback);

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Overrides `model_handler_` with `model_handler`.
  void OverrideModelHandlerForTesting(
      std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler);
#endif

 private:
  // Callback invoked when user observed visits data is ready.
  void OnGotAnnotatedVisits(
      std::vector<history::Cluster> clusters,
      ClustersCallback callback,
      const std::vector<history::AnnotatedVisit> annotated_visits);

  // Callback invoked with the segmentation framework's metrics query data is
  // ready.
  void OnMetricsQueryDataReady(
      base::OnceCallback<void(std::vector<history::Cluster>,
                              std::vector<HistoryClusterMetrics>,
                              HistoryClustersCategoryMetrics)> callback,
      std::vector<history::Cluster> clusters,
      const std::set<std::string> category_ids,
      segmentation_platform::DatabaseClient::ResultStatus status,
      const segmentation_platform::ModelProvider::Request& result);

  // Callback invoked when the cluster related metrics are ready.
  void OnMetricsReady(ClustersCallback callback,
                      std::vector<history::Cluster> clusters,
                      std::vector<HistoryClusterMetrics> cluster_metrics,
                      HistoryClustersCategoryMetrics category_metrics);

  // Callback invoked when all signals for ranking are ready.
  void OnAllSignalsReady(
      ClustersCallback callback,
      std::vector<history::Cluster> clusters,
      const std::vector<HistoryClusterMetrics>& cluster_metrics,
      const HistoryClustersCategoryMetrics& category_metrics,
      bool success,
      std::vector<CartDB::KeyAndValue> active_carts);

  // Runs the fallback heuristic if `model_handler_` is not instantiated or if
  // the model is not available.
  void RunFallbackHeuristic(
      std::vector<history::Cluster> clusters,
      std::unique_ptr<std::vector<HistoryClustersModuleRankingSignals>>
          ranking_signals,
      std::vector<CartDB::KeyAndValue> active_carts,
      ClustersCallback callback);

  // The service to use to query cluster category frequency related data.
  raw_ptr<segmentation_platform::SegmentationPlatformService>
      segmentation_platform_service_;

  // The service used to fetch all available category ids;
  raw_ptr<history::HistoryService> history_service_;

  // The cart service used to check for active carts.
  raw_ptr<CartService> cart_service_;

  // The category boostlist to use.
  const base::flat_set<std::string> category_boostlist_;

  // The number of days to query for cluster specific metrics data preceding the
  // current time.
  int query_day_count_;

  // The model returns a float between -1.0 and 0.
  float threshold_param_;

  // The task tracker for the HistoryService callbacks.
  base::CancelableTaskTracker task_tracker_;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  // Callback invoked when `model_handler_` has completed scoring of `clusters`.
  void OnBatchModelExecutionComplete(
      std::vector<history::Cluster> clusters,
      std::unique_ptr<std::vector<HistoryClustersModuleRankingSignals>>
          ranking_signals,
      std::vector<CartDB::KeyAndValue> active_carts,
      ClustersCallback callback,
      std::vector<float> output);

  // The model handler to use for ranking clusters.
  std::unique_ptr<HistoryClustersModuleRankingModelHandler> model_handler_;
#endif

  base::WeakPtrFactory<HistoryClustersModuleRanker> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_RANKING_HISTORY_CLUSTERS_MODULE_RANKER_H_
