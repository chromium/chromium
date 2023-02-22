// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <vector>

#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"

HistoryClustersPageHandler::HistoryClustersPageHandler(
    mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)), profile_(profile) {}

HistoryClustersPageHandler::~HistoryClustersPageHandler() = default;

void HistoryClustersPageHandler::CallbackWithClusterData(
    GetClusterCallback callback,
    std::vector<history::Cluster> clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  if (clusters.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  auto cluster_mojom = history_clusters::ClusterToMojom(
      TemplateURLServiceFactory::GetForProfile(profile_), clusters.front());
  std::move(callback).Run(std::move(cluster_mojom));
}

void HistoryClustersPageHandler::GetCluster(GetClusterCallback callback) {
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  history_clusters::QueryClustersContinuationParams continuation_params;

  // TODO(b/265301309): Replace `QueryClusters` with the upcoming "Get Cluster"
  // API that will return the most relevant history cluster for a given user.
  // TODO(b/244504329): The first call to QueryClusters may come back with
  // empty data though history clusters may exist.
  fetch_clusters_task_ = history_clusters_service->QueryClusters(
      history_clusters::ClusteringRequestSource::kJourneysPage,
      /*begin_time=*/base::Time(), continuation_params, false,
      base::BindOnce(&HistoryClustersPageHandler::CallbackWithClusterData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
