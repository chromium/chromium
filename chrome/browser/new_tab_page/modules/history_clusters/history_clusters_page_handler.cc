// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_page_handler.h"

#include <string>
#include <tuple>
#include <vector>

#include "base/barrier_callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/cart/cart_service_factory.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/history_clusters/history_clusters_service_factory.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters.mojom.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/side_panel/history_clusters/history_clusters_tab_helper.h"
#include "components/history/core/browser/url_row.h"
#include "components/history_clusters/core/history_cluster_type_utils.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/history_clusters/public/mojom/history_cluster_types.mojom.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/url_util.h"

namespace {

constexpr int kMinRequiredRelatedSearches = 3;

// The minimum number of visits to render a layout is 2 URL visits plus a SRP
// visit.
constexpr int kMinRequiredVisits = 3;

// This enum must match the numbering for NTPHistoryClustersIneligibleReason in
// enums.xml. Do not reorder or remove items, and update kMaxValue when new
// items are added.
enum NTPHistoryClustersIneligibleReason {
  kNone = 0,
  kNoClusters = 1,
  kNonProminent = 2,
  kNoSRPVisit = 3,
  kInsufficientVisits = 4,
  kInsufficientImages = 5,
  kInsufficientRelatedSearches = 6,
  kMaxValue = kInsufficientRelatedSearches,
};

base::flat_set<std::string> GetCategories(const char* feature_param) {
  std::string categories_string = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModuleCategories, feature_param);
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

int GetMinVisitsToShow() {
  static int min_visits = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMinimumVisitsRequired,
      ntp_features::kNtpHistoryClustersModuleMinimumVisitsRequiredParam,
      kMinRequiredVisits);
  if (min_visits < 0) {
    return kMinRequiredVisits;
  }
  return min_visits;
}

int GetMinImagesToShow() {
  static int min_images_to_show = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequired,
      ntp_features::kNtpHistoryClustersModuleMinimumImagesRequiredParam, 1);
  return min_images_to_show;
}

size_t GetMaxClusters() {
  // Even though only one cluster will be shown on the NTP at a time for now,
  // set this to greater than that in case the filtering logic does not match
  // up.
  static int max_clusters = base::GetFieldTrialParamByFeatureAsInt(
      ntp_features::kNtpHistoryClustersModuleMaxClusters,
      ntp_features::kNtpHistoryClustersModuleMaxClustersParam, 5);
  if (max_clusters < 0) {
    return 5;
  }
  return static_cast<size_t>(max_clusters);
}

history_clusters::QueryClustersFilterParams GetFilterParamsFromFeatureFlags() {
  history_clusters::QueryClustersFilterParams filter_params;
  filter_params.min_visits = GetMinVisitsToShow();
  filter_params.min_visits_with_images = GetMinImagesToShow();
  filter_params.categories_allowlist = GetCategories(
      ntp_features::kNtpHistoryClustersModuleCategoriesAllowlistParam);
  filter_params.categories_blocklist = GetCategories(
      ntp_features::kNtpHistoryClustersModuleCategoriesBlocklistParam);
  filter_params.is_search_initiated = true;
  filter_params.has_related_searches = true;
  filter_params.is_shown_on_prominent_ui_surfaces = true;
  filter_params.max_clusters = GetMaxClusters();
  filter_params.categories_boostlist = GetCategories(
      ntp_features::kNtpHistoryClustersModuleCategoriesBoostlistParam);
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

history::ClusterVisit GenerateSampleVisit(
    history::VisitID visit_id,
    const std::string& page_title,
    const GURL& url,
    bool has_url_keyed_image,
    const base::Time visit_time = base::Time::Now()) {
  history::URLRow url_row = history::URLRow(url);
  url_row.set_title(base::UTF8ToUTF16(page_title));
  history::VisitRow visit_row;
  visit_row.visit_id = visit_id;
  visit_row.visit_time = visit_time;
  visit_row.is_known_to_sync = true;
  auto content_annotations = history::VisitContentAnnotations();
  content_annotations.has_url_keyed_image = has_url_keyed_image;
  history::AnnotatedVisit annotated_visit;
  annotated_visit.url_row = std::move(url_row);
  annotated_visit.visit_row = std::move(visit_row);
  annotated_visit.content_annotations = std::move(content_annotations);
  history::ClusterVisit sample_visit;
  sample_visit.normalized_url = url;
  sample_visit.url_for_display =
      history_clusters::ComputeURLForDisplay(url, false);
  sample_visit.annotated_visit = std::move(annotated_visit);

  return sample_visit;
}

history::Cluster GenerateSampleCluster(int num_visits, int num_images) {
  const base::Time current_time = base::Time::Now();
  const std::vector<std::tuple<std::string, GURL, base::Time>>
      kSampleUrlVisitData = {
          {"Pixel 7 Pro - The all-pro Google phone.",
           GURL("https://store.google.com/product/pixel_7?hl=en-US"),
           current_time - base::Minutes(1)},
          {"Pixel Buds Pro - How premium sounds.",
           GURL("https://store.google.com/product/pixel_buds_pro?hl=en-US"),
           current_time - base::Hours(1)},
          {"Pixel Watch - Help by Google. Health by Fitbit.",
           GURL("https://store.google.com/product/google_pixel_watch?hl=en-US"),
           current_time - base::Hours(4)},
          {"Next Door Bells - Know who's knocking.",
           GURL("https://store.google.com/product/nest_doorbell?hl=en-US"),
           current_time - base::Hours(8)}};

  std::vector<history::ClusterVisit> sample_visits;
  for (int i = 0; i < num_visits; i++) {
    const std::tuple<std::string, GURL, base::Time> kSampleData =
        kSampleUrlVisitData.at(i % kSampleUrlVisitData.size());
    sample_visits.push_back(GenerateSampleVisit(
        i, std::get<0>(kSampleData), std::get<1>(kSampleData), (i < num_images),
        std::get<2>(kSampleData)));
  }

  std::string kSampleSearchQuery = "google store products";
  url::RawCanonOutputT<char> encoded_query;
  url::EncodeURIComponent(kSampleSearchQuery.c_str(),
                          kSampleSearchQuery.length(), &encoded_query);
  sample_visits.insert(
      sample_visits.begin(),
      GenerateSampleVisit(
          0, kSampleSearchQuery + " - Google Search",
          GURL("https://www.google.com/search?q=" +
               std::string(encoded_query.data(), encoded_query.length())),
          false));

  return history::Cluster(
      0, sample_visits, {},
      /*should_show_on_prominent_ui_surfaces=*/true,
      /*label=*/
      l10n_util::GetStringFUTF16(
          IDS_HISTORY_CLUSTERS_CLUSTER_LABEL_SEARCH_TERMS,
          base::UTF8ToUTF16(kSampleSearchQuery)),
      /*raw_label=*/base::UTF8ToUTF16(kSampleSearchQuery), {},
      {"new google products", "google devices", "google stuff"}, 0);
}

}  // namespace

HistoryClustersPageHandler::HistoryClustersPageHandler(
    mojo::PendingReceiver<ntp::history_clusters::mojom::PageHandler>
        pending_receiver,
    content::WebContents* web_contents)
    : receiver_(this, std::move(pending_receiver)),
      profile_(Profile::FromBrowserContext(web_contents->GetBrowserContext())),
      web_contents_(web_contents),
      filter_params_(GetFilterParamsFromFeatureFlags()),
      cart_service_(CartServiceFactory::GetForProfile(profile_)) {}

HistoryClustersPageHandler::~HistoryClustersPageHandler() = default;

void HistoryClustersPageHandler::CallbackWithClusterData(
    GetClusterCallback callback,
    std::vector<history::Cluster> clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  const TemplateURLService* template_url_service =
      TemplateURLServiceFactory::GetForProfile(profile_);
  if (!template_url_service) {
    return;
  }

  history_clusters::CoalesceRelatedSearches(clusters);

  // Cull clusters that do not have the minimum number of visits with and
  // without images to be eligible for display.
  NTPHistoryClustersIneligibleReason ineligible_reason =
      clusters.empty() ? kNoClusters : kNone;
  base::EraseIf(clusters, [&](auto& cluster) {
    // Cull non prominent clusters.
    if (!cluster.should_show_on_prominent_ui_surfaces) {
      ineligible_reason = kNonProminent;
      return true;
    }

    // Cull clusters whose visits don't have at least one SRP.
    const TemplateURL* default_search_provider =
        template_url_service->GetDefaultSearchProvider();
    auto srp_visits_it = std::find_if(
        cluster.visits.begin(), cluster.visits.end(), [&](auto& visit) {
          return default_search_provider->IsSearchURL(
              visit.normalized_url, template_url_service->search_terms_data());
        });
    if (srp_visits_it == cluster.visits.end()) {
      ineligible_reason = kNoSRPVisit;
      return true;
    }

    // Ensure visits contains at most one SRP visit and its the first one in the
    // list.
    history::ClusterVisit first_srp_visit = *srp_visits_it;
    base::EraseIf(cluster.visits, [&](auto& visit) {
      return default_search_provider->IsSearchURL(
          visit.normalized_url, template_url_service->search_terms_data());
    });
    cluster.visits.insert(cluster.visits.begin(), first_srp_visit);

    // Cull visits that have a zero relevance score.
    base::EraseIf(cluster.visits,
                  [&](auto& visit) { return visit.score == 0.0; });

    int visits_with_images = std::accumulate(
        cluster.visits.begin(), cluster.visits.end(), 0,
        [](const auto& i, const auto& v) {
          return i + int(v.annotated_visit.content_annotations
                             .has_url_keyed_image &&
                         v.annotated_visit.visit_row.is_known_to_sync);
        });

    if (cluster.visits.size() < kMinRequiredVisits) {
      ineligible_reason = kInsufficientVisits;
      return true;
    }

    if (visits_with_images < GetMinImagesToShow()) {
      ineligible_reason = kInsufficientImages;
      return true;
    }

    // Cull clusters that do not have the minimum required number of related
    // searches to be eligible for display.
    if (cluster.related_searches.size() < kMinRequiredRelatedSearches) {
      ineligible_reason = kInsufficientRelatedSearches;
      return true;
    }

    return false;
  });

  base::UmaHistogramEnumeration("NewTabPage.HistoryClusters.IneligibleReason",
                                ineligible_reason);
  base::UmaHistogramBoolean("NewTabPage.HistoryClusters.HasClusterToShow",
                            !clusters.empty());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumClusterCandidates",
                              clusters.size());

  if (clusters.empty()) {
    std::move(callback).Run(nullptr);
    return;
  }

  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumVisits",
                              clusters.front().visits.size());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumRelatedSearches",
                              clusters.front().related_searches.size());

  history::Cluster top_cluster = clusters.front();
  auto cluster_mojom =
      history_clusters::ClusterToMojom(template_url_service, top_cluster);
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
  const std::string fake_data_param = base::GetFieldTrialParamValueByFeature(
      ntp_features::kNtpHistoryClustersModule,
      ntp_features::kNtpHistoryClustersModuleDataParam);

  if (!fake_data_param.empty()) {
    const std::vector<std::string> kFakeDataParams = base::SplitString(
        fake_data_param, ",", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
    if (kFakeDataParams.size() != 2) {
      LOG(ERROR) << "Invalid history clusters fake data selection parameter "
                    "format.";
      std::move(callback).Run(nullptr);
      return;
    }

    int num_visits;
    int num_images;
    if (!base::StringToInt(kFakeDataParams.at(0), &num_visits) ||
        !base::StringToInt(kFakeDataParams.at(1), &num_images) ||
        num_visits < num_images) {
      std::move(callback).Run(nullptr);
      return;
    }

    std::move(callback).Run(history_clusters::ClusterToMojom(
        TemplateURLServiceFactory::GetForProfile(profile_),
        GenerateSampleCluster(num_visits, num_images)));
    return;
  }

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

  if (tab_indices.size() > 1) {
    model->ActivateTabAt(tab_indices.at(1));
  }
}

void HistoryClustersPageHandler::DismissCluster(
    const std::vector<history_clusters::mojom::URLVisitPtr> visits) {
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
}
