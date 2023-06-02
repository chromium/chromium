// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_service.h"

#include "base/barrier_callback.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_split.h"
#include "chrome/browser/cart/cart_service.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/history_clusters_module_util.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranker.h"
#include "chrome/browser/new_tab_page/modules/history_clusters/ranking/history_clusters_module_ranking_signals.h"
#include "chrome/browser/new_tab_page/new_tab_page_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "components/history/core/browser/history_types.h"
#include "components/history_clusters/core/history_clusters_service.h"
#include "components/history_clusters/core/history_clusters_service_task.h"
#include "components/history_clusters/core/history_clusters_types.h"
#include "components/history_clusters/core/history_clusters_util.h"
#include "components/search/ntp_features.h"
#include "components/search_engines/template_url.h"
#include "components/search_engines/template_url_service.h"

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

HistoryClustersModuleService::HistoryClustersModuleService(
    history_clusters::HistoryClustersService* history_clusters_service,
    CartService* cart_service,
    TemplateURLService* template_url_service,
    OptimizationGuideKeyedService* optimization_guide_keyed_service)
    : filter_params_(GetFilterParamsFromFeatureFlags()),
      max_clusters_to_return_(GetMaxClusters()),
      category_boostlist_(GetCategories(
          ntp_features::kNtpHistoryClustersModuleCategoriesBoostlistParam)),
      should_fetch_clusters_until_exhausted_(base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleFetchClustersUntilExhausted)),
      history_clusters_service_(history_clusters_service),
      cart_service_(cart_service),
      template_url_service_(template_url_service) {
  if (base::FeatureList::IsEnabled(
          ntp_features::kNtpHistoryClustersModuleUseModelRanking) &&
      optimization_guide_keyed_service) {
    module_ranker_ = std::make_unique<HistoryClustersModuleRanker>(
        optimization_guide_keyed_service, cart_service_, category_boostlist_);
  }
}
HistoryClustersModuleService::~HistoryClustersModuleService() = default;

void HistoryClustersModuleService::GetClusters(GetClustersCallback callback) {
  if (!history_clusters_service_->IsJourneysEnabledAndVisible()) {
    std::move(callback).Run({}, {});
    return;
  }

  if (!template_url_service_) {
    std::move(callback).Run({}, {});
    return;
  }

  GetClusters(GetBeginTime(),
              history_clusters::QueryClustersContinuationParams(),
              std::move(callback));
}

void HistoryClustersModuleService::GetClusters(
    base::Time begin_time,
    history_clusters::QueryClustersContinuationParams continuation_params,
    GetClustersCallback callback) {
  // TODO(crbug/1442619): Encapsulate work done by this method in a task that
  // gets returned to the caller.

  size_t task_id = task_id_++;
  std::unique_ptr<history_clusters::HistoryClustersServiceTask>
      query_clusters_task = history_clusters_service_->QueryClusters(
          history_clusters::ClusteringRequestSource::kNewTabPage,
          filter_params_, begin_time, continuation_params,
          /*recluster=*/false,
          base::BindOnce(&HistoryClustersModuleService::OnGetFilteredClusters,
                         weak_ptr_factory_.GetWeakPtr(), task_id, begin_time,
                         std::move(callback)));
  in_progress_query_clusters_tasks_.insert_or_assign(
      task_id, std::move(query_clusters_task));
}

void HistoryClustersModuleService::OnGetFilteredClusters(
    size_t pending_task_id,
    base::Time begin_time,
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    history_clusters::QueryClustersContinuationParams continuation_params) {
  base::UmaHistogramBoolean(
      "NewTabPage.HistoryClusters.ExhaustedEligibleClusters",
      continuation_params.exhausted_all_visits);

  in_progress_query_clusters_tasks_.erase(pending_task_id);

  // Within each cluster, sort visits.
  for (auto& cluster : clusters) {
    history_clusters::StableSortVisits(cluster.visits);
  }

  // Do additional filtering on clusters.
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
        template_url_service_->GetDefaultSearchProvider();
    auto srp_visits_it = std::find_if(
        cluster.visits.begin(), cluster.visits.end(), [&](auto& visit) {
          return default_search_provider->IsSearchURL(
              visit.normalized_url, template_url_service_->search_terms_data());
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
          visit.normalized_url, template_url_service_->search_terms_data());
    });
    cluster.visits.insert(cluster.visits.begin(), first_srp_visit);

    // Cull visits that have a zero relevance score, are Hidden, or Done.
    base::EraseIf(cluster.visits, [&](auto& visit) {
      return visit.score == 0.0 ||
             visit.interaction_state ==
                 history::ClusterVisit::InteractionState::kHidden ||
             visit.interaction_state ==
                 history::ClusterVisit::InteractionState::kDone;
    });

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

  bool should_fetch_more_clusters = should_fetch_clusters_until_exhausted_ &&
                                    clusters.empty() &&
                                    !continuation_params.exhausted_all_visits;
  if (!should_fetch_more_clusters) {
    // Only record metrics if we are ready to rank clusters or have no more
    // clusters to query for.
    base::UmaHistogramEnumeration("NewTabPage.HistoryClusters.IneligibleReason",
                                  ineligible_reason);
    base::UmaHistogramBoolean("NewTabPage.HistoryClusters.HasClusterToShow",
                              !clusters.empty());
    base::UmaHistogramCounts100(
        "NewTabPage.HistoryClusters.NumClusterCandidates", clusters.size());
  }

  if (clusters.empty()) {
    if (should_fetch_more_clusters) {
      // If no clusters to show and visits have not been exhausted, fetch for
      // more clusters.
      // TODO(crbug/1442074): This logic should probably change to just keep
      // iterating until its over for the next phase of this project.
      GetClusters(begin_time, continuation_params, std::move(callback));
    } else {
      std::move(callback).Run(/*clusters=*/{}, /*ranking_signals=*/{});
    }

    return;
  }

  if (module_ranker_) {
    module_ranker_->RankClusters(
        std::move(clusters),
        base::BindOnce(&HistoryClustersModuleService::OnGetRankedClusters,
                       weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
  } else {
    SortClustersUsingHeuristic(category_boostlist_, clusters);
    OnGetRankedClusters(std::move(callback), std::move(clusters),
                        /*ranking_signals=*/{});
  }
}

void HistoryClustersModuleService::OnGetRankedClusters(
    GetClustersCallback callback,
    std::vector<history::Cluster> clusters,
    base::flat_map<int64_t, HistoryClustersModuleRankingSignals>
        ranking_signals) {
  // Record metrics for top cluster.
  history::Cluster top_cluster = clusters.front();
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumVisits",
                              top_cluster.visits.size());
  base::UmaHistogramCounts100("NewTabPage.HistoryClusters.NumRelatedSearches",
                              top_cluster.related_searches.size());

  // Cull to max clusters to return.
  if (clusters.size() > max_clusters_to_return_) {
    clusters.resize(max_clusters_to_return_);
  }

  std::move(callback).Run(std::move(clusters), std::move(ranking_signals));

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
