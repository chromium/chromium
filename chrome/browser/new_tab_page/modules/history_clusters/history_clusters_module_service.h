// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/keyed_service/core/keyed_service.h"

namespace history {
struct Cluster;
}  // namespace history

namespace history_clusters {
class HistoryClustersService;
class HistoryClustersServiceTask;
struct QueryClustersContinuationParams;
}  // namespace history_clusters

class CartService;
class HistoryClustersModuleRanker;
class HistoryClustersModuleRankingSignals;
class OptimizationGuideKeyedService;
class TemplateURLService;

// Handles requests to get clusters for the History Clusters Module.
class HistoryClustersModuleService : public KeyedService {
 public:
  HistoryClustersModuleService(const HistoryClustersModuleService&) = delete;
  HistoryClustersModuleService(
      history_clusters::HistoryClustersService* history_clusters_service,
      CartService* cart_service,
      TemplateURLService* template_url_service,
      OptimizationGuideKeyedService* optimization_guide_keyed_service);
  ~HistoryClustersModuleService() override;

  using GetClustersCallback = base::OnceCallback<void(
      std::vector<history::Cluster>,
      base::flat_map<int64_t, HistoryClustersModuleRankingSignals>)>;

  // Returns the pending task to query clusters and invokes `callback` when
  // clusters are ready.
  //
  // Virtual for testing.
  virtual std::unique_ptr<history_clusters::HistoryClustersServiceTask>
  GetClusters(GetClustersCallback callback);

 private:
  // Callback invoked when `history_clusters_service_` returns filtered
  // clusters.
  void OnGetFilteredClusters(
      GetClustersCallback callback,
      std::vector<history::Cluster> clusters,
      history_clusters::QueryClustersContinuationParams continuation_params);

  // Callback invoked when `module_ranker_` returns ranked clusters.
  void OnGetRankedClusters(
      GetClustersCallback callback,
      std::vector<history::Cluster> clusters,
      base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
          ranking_signals);

  // The filtering parameters to use for all calls to fetch clusters.
  const history_clusters::QueryClustersFilterParams filter_params_;

  // The max number of clusters to return.
  const size_t max_clusters_to_return_;

  // The categories to boost.
  const base::flat_set<std::string> category_boostlist_;

  raw_ptr<history_clusters::HistoryClustersService> history_clusters_service_;
  raw_ptr<CartService> cart_service_;
  raw_ptr<TemplateURLService> template_url_service_;
  std::unique_ptr<HistoryClustersModuleRanker> module_ranker_;

  // Weak pointers issued from this factory never get invalidated before the
  // service is destroyed.
  base::WeakPtrFactory<HistoryClustersModuleService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_MODULE_SERVICE_H_
