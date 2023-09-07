// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_V2_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_V2_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/discount/discount.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_layout_type.mojom.h"
#include "chrome/browser/new_tab_page/modules/v2/history_clusters/history_clusters_v2.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

class CartProcessor;
class DiscountProcessor;
class HistoryClustersModuleRankingMetricsLogger;
class HistoryClustersModuleRankingSignals;
class Profile;

namespace content {
class WebContents;
}  // namespace content

class HistoryClustersPageHandlerV2
    : public ntp::history_clusters_v2::mojom::PageHandler {
 public:
  HistoryClustersPageHandlerV2(
      mojo::PendingReceiver<ntp::history_clusters_v2::mojom::PageHandler>
          pending_receiver,
      content::WebContents* web_contents);
  HistoryClustersPageHandlerV2(const HistoryClustersPageHandlerV2&) = delete;
  HistoryClustersPageHandlerV2& operator=(const HistoryClustersPageHandlerV2&) =
      delete;
  ~HistoryClustersPageHandlerV2() override;

  // mojom::PageHandler:
  void GetClusters(GetClustersCallback callback) override;
  void GetCartForCluster(history_clusters::mojom::ClusterPtr cluster,
                         GetCartForClusterCallback callback) override;
  void GetDiscountsForCluster(history_clusters::mojom::ClusterPtr cluster,
                              GetDiscountsForClusterCallback callback) override;
  void ShowJourneysSidePanel(const std::string& query) override;
  void RecordClick(int64_t cluster_id) override;
  void RecordLayoutTypeShown(
      ntp::history_clusters::mojom::LayoutType layout_type,
      int64_t cluster_id) override;
  void UpdateClusterVisitsInteractionState(
      const std::vector<history_clusters::mojom::URLVisitPtr> visits,
      const history_clusters::mojom::InteractionState state) override;

 private:
  // Forward the most relevant history clusters to the callback if any.
  void CallbackWithClusterData(
      GetClustersCallback callback,
      std::vector<history::Cluster> clusters,
      base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
          ranking_signals);

  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;
  base::CancelableTaskTracker update_visits_task_tracker_;
  std::unique_ptr<CartProcessor> cart_processor_;
  std::unique_ptr<DiscountProcessor> discount_processor_;
  // The logger used to record metrics related to module ranking scoped to
  // `this`. Will be nullptr until clusters are received and ranking signals are
  // returned in the callback.
  std::unique_ptr<HistoryClustersModuleRankingMetricsLogger>
      ranking_metrics_logger_;

  // Located at the end of the list of member variables to ensure the WebUI page
  // is disconnected before other members are destroyed.
  mojo::Receiver<ntp::history_clusters_v2::mojom::PageHandler> receiver_;

  base::WeakPtrFactory<HistoryClustersPageHandlerV2> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_V2_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_V2_H_
