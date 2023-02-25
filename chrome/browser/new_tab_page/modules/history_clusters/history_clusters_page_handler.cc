// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <vector>

#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
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
    content::WebContents* web_contents)
    : receiver_(this, std::move(pending_receiver)),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      filter_params_(GetFilterParamsFromFeatureFlags()) {}

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

  auto cluster_mojom = history_clusters::ClusterToMojom(
      TemplateURLServiceFactory::GetForProfile(profile_), clusters.front());
  std::move(callback).Run(std::move(cluster_mojom));
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

void HistoryClustersPageHandler::ShowJourneysSidePanel(
    const std::string& query) {
  // TODO(crbug.com/1341399): Revisit integration with the side panel once the
  // referenced bug is resolved.
  auto* history_clusters_tab_helper =
      side_panel::HistoryClustersTabHelper::FromWebContents(web_contents_);
  history_clusters_tab_helper->ShowJourneysSidePanel(query);
}

void HistoryClustersPageHandler::OpenUrlsInTabGroup(
    const std::vector<GURL>& urls) {
  if (urls.empty()) {
    return;
  }

  Browser* browser = chrome::FindLastActive();
  if (!browser) {
    return;
  }

  browser->OpenURL({urls.front(), content::Referrer(),
                    WindowOpenDisposition::CURRENT_TAB,
                    ui::PAGE_TRANSITION_AUTO_BOOKMARK,
                    /*is_renderer_initiated=*/false});

  auto* model = browser->tab_strip_model();
  std::vector<int> tab_indices;
  tab_indices.reserve(urls.size());
  for (size_t i = 1; i < urls.size(); i++) {
    auto* opened_web_contents = browser->OpenURL(content::OpenURLParams(
        urls[i], content::Referrer(), WindowOpenDisposition::NEW_BACKGROUND_TAB,
        ui::PAGE_TRANSITION_AUTO_BOOKMARK, false));

    // Only add those tabs to a new group that actually opened in this
    // browser.
    const int tab_index = model->GetIndexOfWebContents(opened_web_contents);
    if (tab_index != TabStripModel::kNoTab) {
      tab_indices.push_back(tab_index);
    }
  }

  tab_indices.insert(tab_indices.begin(), model->GetIndexOfWebContents(
                                              model->GetActiveWebContents()));
  model->AddToNewGroup(tab_indices);
}
