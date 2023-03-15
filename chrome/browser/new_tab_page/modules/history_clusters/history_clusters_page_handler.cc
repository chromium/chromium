// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <vector>

#include "base/barrier_callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/search/ntp_features.h"

namespace {

base::flat_set<std::string> GetCategories() {
  std::string categories_string = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModuleCategories,
      ntp_features::kNtpHistoryClustersModuleCategoriesParam);
  if (categories_string.empty()) {
    return {};
  }

  auto categories = base::SplitString(categories_string, ",",
                                      base::WhitespaceHandling::TRIM_WHITESPACE,
                                      base::SplitResult::SPLIT_WANT_NONEMPTY);

  return categories.empty() ? base::flat_set<std::string>()
                            : base::flat_set<std::string>(categories.begin(),
                                                          categories.end());
}

int GetMinImagesToShow() {
  static int min_images_to_show = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequired,
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequiredParam, 2);
  return min_images_to_show;
}

history_clusters::QueryClustersFilterParams GetFilterParamsFromFeatureFlags() {
  history_clusters::QueryClustersFilterParams filter_params;
  filter_params.min_visits_with_images = GetMinImagesToShow();
  filter_params.categories = GetCategories();
  filter_params.is_search_initiated = true;
  filter_params.has_related_searches = true;
  filter_params.is_shown_on_prominent_ui_surfaces = true;
  // TODO(b/265301665): Add max clusters param when actually showing in the UI.
  return filter_params;
}

base::Time GetBeginTime() {
  static int hours_to_look_back = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleBeginTimeDuration,
      ntp_features::kNtpHistoryClustersModuleBeginTimeDurationHoursParam, 24);
  if (hours_to_look_back <= 0) {
    hours_to_look_back = 24;
  }

  return base::Time::Now() - base::Hours(hours_to_look_back);
}

}  // namespace

HistoryClustersPageHandler::HistoryClustersPageHandler(
    mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>
        pending_receiver,
    Profile* profile)
    : receiver_(this, std::move(pending_receiver)),
      profile_(profile),
      filter_params_(GetFilterParamsFromFeatureFlags()),
      cart_service_(CartServiceFactory::GetForProfile(profile_)) {}

HistoryClustersPageHandler::~HistoryClustersPageHandler() = default;

void HistoryClustersPageHandler::CallbackWithClusterData(
    GetClusterCallback callback,
    std::vector<history::Cluster> clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  base::UmaHistogramBoolean("NewTabPage.HistoryClusters.HasClusterToShow",
                            !clusters.empty());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumClusterCandidates",
                              clusters.size());

  if (clusters.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  history::Cluster top_cluster = clusters.front();
  auto cluster_mojom = history_clusters::ClusterToMojom(
      TemplateURLServiceFactory::GetForProfile(profile_), top_cluster);
  std::move(callback).Run(std::move(cluster_mojom));

  if (!IsCartModuleEnabled() || !cart_service_) {
    return;
  }
  const auto metrics_callback = base::BarrierCallback<bool>(
      top_cluster.visits.size(),
      base::BindOnce([](const std::vector<bool>& results) {
        bool has_cart = false;
        for (bool result : results) {
          has_cart = has_cart || result;
        }
        base::UmaHistogramBoolean(
            "NewTabPage.HistoryClusters.HasCartForTopCluster", has_cart);
      }));
  for (auto& visit : top_cluster.visits) {
    cart_service_->HasActiveCartForURL(visit.normalized_url, metrics_callback);
  }
}

void HistoryClustersPageHandler::GetCluster(GetClusterCallback callback) {
  auto* history_clusters_service =
      HistoryClustersServiceFactory::GetForBrowserContext(profile_);
  history_clusters::QueryClustersContinuationParams continuation_params;

  // TODO(b/244504329): The first call to QueryClusters may come back with
  // empty data though history clusters may exist.
  fetch_clusters_task_ = history_clusters_service->QueryClusters(
      history_clusters::ClusteringRequestSource::kNewTabPage, filter_params_,
      GetBeginTime(), continuation_params,
      /*recluster=*/false,
      base::BindOnce(&HistoryClustersPageHandler::CallbackWithClusterData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}
