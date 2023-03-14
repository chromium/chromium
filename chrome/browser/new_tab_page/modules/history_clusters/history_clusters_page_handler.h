// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_H_

#include <memory>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/receiver.h"

class Profile;
class GURL;
class CartService;

namespace content {
class WebContents;
}  // namespace content

namespace history_clusters {
class HistoryClustersServiceTask;
}  // namespace history_clusters

class HistoryClustersPageHandler
    : public ntp::history_clusters::mojom::PageHandler {
 public:
  HistoryClustersPageHandler(
      mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>
          pending_receiver,
      content::WebContents* web_contents);
  HistoryClustersPageHandler(const HistoryClustersPageHandler&) = delete;
  HistoryClustersPageHandler& operator=(const HistoryClustersPageHandler&) =
      delete;
  ~HistoryClustersPageHandler() override;

  // mojom::PageHandler:
  void GetCluster(GetClusterCallback callback) override;
  void ShowJourneysSidePanel(const std::string& query) override;
  void OpenUrlsInTabGroup(const std::vector<GURL>&) override;
  void DismissCluster(
      const std::vector<history_clusters::mojom::URLVisitPtr> visits) override;

 private:
  // Forward the most relevant history cluster to the callback if any.
  void CallbackWithClusterData(
      GetClusterCallback callback,
      std::vector<history::Cluster> clusters,
      history_clusters::QueryClustersContinuationParams continuation_params);

  mojo::Receiver<ntp::history_clusters::mojom::PageHandler> receiver_;
  raw_ptr<Profile> profile_;
  raw_ptr<content::WebContents> web_contents_;

  // The filtering parameters to use for all calls to fetch clusters.
  history_clusters::QueryClustersFilterParams filter_params_;

  // Tracks the current fetch clusters task. Will be `nullptr` or
  // `Done()` will be true if there is no ongoing task.
  std::unique_ptr<history_clusters::HistoryClustersServiceTask>
      fetch_clusters_task_;
  base::CancelableTaskTracker hide_visits_task_tracker_;
  raw_ptr<CartService> cart_service_;

  base::WeakPtrFactory<HistoryClustersPageHandler> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_MODULES_HISTORY_CLUSTERS_HISTORY_CLUSTERS_PAGE_HANDLER_H_
