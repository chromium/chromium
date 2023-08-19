// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/cart/cart_processor.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_metrics_logger.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_group.h"
#include "chrome/browser/ui/tabs/tab_group_model.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search/ntp_features.h"
#include "components/strings/grit/components_strings.h"

namespace {

// The minimum number of visits to render a layout is 2 URL visits plus a SRP
// visit.
constexpr int kMinRequiredVisits = 3;

constexpr int kMinRequiredRelatedSearches = 3;

}  // namespace

HistoryClustersPageHandler::HistoryClustersPageHandler(
    mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>
        pending_receiver,
    content::WebContents* web_contents)
    : profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      ranking_metrics_logger_(
          std::make_unique<HistoryClustersModuleRankingMetricsLogger>(
              web_contents_->GetPrimaryMainFrame()->GetPageUkmSourceId())),
      receiver_(this, std::move(pending_receiver)) {
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpChromeCartInHistoryClusterModule)) {
    cart_processor_ = std::make_unique<CartProcessor>(
        CartServiceFactory::GetForProfile(profile_));
  }
}

HistoryClustersPageHandler::~HistoryClustersPageHandler() {
  receiver_.reset();

  ranking_metrics_logger_->RecordUkm(/*record_in_cluster_id_order=*/false);
}

void HistoryClustersPageHandler::CallbackWithClusterData(
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
        ranking_signals) {
  if (clusters.empty()) {
    std::move(callback).Run({});
    return;
  }

  ranking_metrics_logger_->AddSignals(std::move(ranking_signals));

  std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
  for (const auto& cluster : clusters) {
    clusters_mojom.push_back(history_clusters::ClusterToMojom(
        TemplateURLServiceFactory::GetForProfile(profile_), cluster));
  }
  std::move(callback).Run(std::move(clusters_mojom));
}

void HistoryClustersPageHandler::GetClusters(GetClustersCallback callback) {
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModule,
      ntp_features::kNtpHistoryClustersModuleDataParam);

  if (!fake_data_param.empty()) {
    const std::vector<std::string> kFakeDataParams = base::SplitString(
        fake_data_param, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (kFakeDataParams.size() != 3) {
      LOG(ERROR) << "Invalid history clusters fake data selection parameter "
                    "format.";
      std::move(callback).Run({});
      return;
    }

    int num_clusters;
    int num_visits;
    int num_images;
    if (!base::StringToInt(kFakeDataParams.at(0), &num_clusters) ||
        !base::StringToInt(kFakeDataParams.at(1), &num_visits) ||
        !base::StringToInt(kFakeDataParams.at(2), &num_images) ||
        num_visits < num_images) {
      std::move(callback).Run({});
      return;
    }

    std::vector<history_clusters::mojom::ClusterPtr> clusters_mojom;
    for (int i = 0; i < num_clusters; i++) {
      clusters_mojom.push_back(history_clusters::ClusterToMojom(
          TemplateURLServiceFactory::GetForProfile(profile_),
          GenerateSampleCluster(i, num_visits, num_images)));
    }
    std::move(callback).Run(std::move(clusters_mojom));
    return;
  }

  auto* history_clusters_module_service =
      HistoryClustersModuleServiceFactory::GetForProfile(profile_);
  if (!history_clusters_module_service) {
    std::move(callback).Run({});
    return;
  }

  history_clusters::QueryClustersFilterParams filter_params =
      CreateFilterParamsFromFeatureFlags(kMinRequiredVisits,
                                         kMinRequiredRelatedSearches);
  history_clusters_module_service->GetClusters(
      std::move(filter_params),
      static_cast<size_t>(kMinRequiredRelatedSearches),
      base::BindOnce(&HistoryClustersPageHandler::CallbackWithClusterData,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void HistoryClustersPageHandler::GetCartForCluster(
    history_clusters::mojom::ClusterPtr cluster,
    GetCartForClusterCallback callback) {
  if (!base::FeatureList::IsEnabled(
          ntp_features::kNtpChromeCartInHistoryClusterModule)) {
    std::move(callback).Run(nullptr);
    return;
  }
  DCHECK(cart_processor_);
  cart_processor_->GetCartForCluster(std::move(cluster), std::move(callback));
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
    const std::vector<GURL>& urls,
    const absl::optional<std::string>& tab_group_name) {
  // This method is different from HistoryClustersHandler::OpenUrlsInTabGroup:
  //  - It takes over the current tab instead of opening new background tabs.
  //  - It inserts the new tabs into the location of the current tab.

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

  auto new_group_id = model->AddToNewGroup(tab_indices);
  if (!new_group_id.is_empty() && tab_group_name) {
    if (auto* group_model = model->group_model()) {
      auto* tab_group = group_model->GetTabGroup(new_group_id);
      // Copy and modify the existing visual data with a new title.
      tab_groups::TabGroupVisualData visual_data = *tab_group->visual_data();
      visual_data.SetTitle(base::UTF8ToUTF16(*tab_group_name));
      tab_group->SetVisualData(visual_data);
    }
  }

  if (tab_indices.size() > 1) {
    model->ActivateTabAt(tab_indices.at(1));
  }
}

void HistoryClustersPageHandler::DismissCluster(
    const std::vector<history_clusters::mojom::URLVisitPtr> visits,
    int64_t cluster_id) {
  if (visits.empty()) {
    return;
  }

  std::vector<history::VisitID> visit_ids;
  base::ranges::transform(
      visits, std::back_inserter(visit_ids),
      [](const auto& url_visit_ptr) { return url_visit_ptr->visit_id; });

  auto* history_service = HistoryServiceFactory::GetForProfile(
      profile_, ServiceAccessType::EXPLICIT_ACCESS);
  history_service->HideVisits(visit_ids, base::BindOnce([]() {}),
                              &hide_visits_task_tracker_);
  ranking_metrics_logger_->SetDismissed(cluster_id);
}

void HistoryClustersPageHandler::RecordClick(int64_t cluster_id) {
  ranking_metrics_logger_->SetClicked(cluster_id);
}

void HistoryClustersPageHandler::RecordDisabled(int64_t cluster_id) {
  ranking_metrics_logger_->SetDisabled(cluster_id);
}

void HistoryClustersPageHandler::RecordLayoutTypeShown(
    ntp::history_clusters::mojom::LayoutType layout_type,
    int64_t cluster_id) {
  ranking_metrics_logger_->SetLayoutTypeShown(layout_type, cluster_id);
}
