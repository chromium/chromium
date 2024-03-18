// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/ranges/algorithm.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/preconnect_manager.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace predictors {

namespace {

using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;

RedirectStatus GetPredictionRedirectStatus(const GURL& initial_url,
                                           const GURL& main_frame_url,
                                           const std::string& prediction_key,
                                           bool is_redirected,
                                           bool is_host) {
  if (main_frame_url == initial_url) {
    // The actual navigation wasn't redirected.
    return is_redirected ? RedirectStatus::NO_REDIRECT_BUT_PREDICTED
                         : RedirectStatus::NO_REDIRECT;
  }

  if (!is_redirected)
    return RedirectStatus::REDIRECT_NOT_PREDICTED;

  const std::string& main_frame_key =
      is_host ? main_frame_url.host() : main_frame_url.spec();
  return main_frame_key == prediction_key
             ? RedirectStatus::REDIRECT_CORRECTLY_PREDICTED
             : RedirectStatus::REDIRECT_WRONG_PREDICTED;
}

std::string GetHistogramNameForHintOrigin(HintOrigin hint_origin,
                                          const char* histogram_base) {
  return base::StringPrintf("%s.%s", histogram_base,
                            GetStringNameForHintOrigin(hint_origin).c_str());
}

// Reports histograms for the accuracy of the prediction. Returns the number of
// origins that were correctly predicted.
size_t ReportPreconnectPredictionAccuracy(
    const PreconnectPrediction& prediction,
    const PageRequestSummary& summary,
    HintOrigin hint_origin) {
  if (prediction.requests.empty() || summary.origins.empty())
    return 0;

  const auto& actual_origins = summary.origins;

  size_t correctly_predicted_count = base::ranges::count_if(
      prediction.requests, [&actual_origins](const PreconnectRequest& request) {
        return actual_origins.find(request.origin) != actual_origins.end();
      });
  size_t precision_percentage =
      (100 * correctly_predicted_count) / prediction.requests.size();
  size_t recall_percentage =
      (100 * correctly_predicted_count) / actual_origins.size();

  base::UmaHistogramPercentageObsoleteDoNotUse(
      GetHistogramNameForHintOrigin(
          hint_origin, internal::kLoadingPredictorPreconnectLearningPrecision),
      precision_percentage);
  base::UmaHistogramPercentageObsoleteDoNotUse(
      GetHistogramNameForHintOrigin(
          hint_origin, internal::kLoadingPredictorPreconnectLearningRecall),
      recall_percentage);
  base::UmaHistogramCounts100(
      GetHistogramNameForHintOrigin(
          hint_origin, internal::kLoadingPredictorPreconnectLearningCount),
      prediction.requests.size());

  RedirectStatus redirect_status = GetPredictionRedirectStatus(
      summary.initial_url, summary.main_frame_url, prediction.host,
      prediction.is_redirected, true /* is_host */);

  base::UmaHistogramEnumeration(
      GetHistogramNameForHintOrigin(
          hint_origin,
          internal::kLoadingPredictorPreconnectLearningRedirectStatus),
      redirect_status, RedirectStatus::MAX);

  return correctly_predicted_count;
}

void ReportPreconnectAccuracy(
    const PreconnectStats& stats,
    const std::map<url::Origin, OriginRequestSummary>& requests) {
  if (stats.requests_stats.empty())
    return;

  int preresolve_hits_count = 0;
  int preresolve_misses_count = 0;
  int preconnect_hits_count = 0;
  int preconnect_misses_count = 0;

  for (const auto& request_stats : stats.requests_stats) {
    bool hit = requests.find(request_stats.origin) != requests.end();
    bool preconnect = request_stats.was_preconnected;

    preresolve_hits_count += hit;
    preresolve_misses_count += !hit;
    preconnect_hits_count += preconnect && hit;
    preconnect_misses_count += preconnect && !hit;
  }

  int total_preresolves = preresolve_hits_count + preresolve_misses_count;
  int total_preconnects = preconnect_hits_count + preconnect_misses_count;
  DCHECK_EQ(static_cast<int>(stats.requests_stats.size()),
            preresolve_hits_count + preresolve_misses_count);
  DCHECK_GT(total_preresolves, 0);

  size_t preresolve_hits_percentage =
      (100 * preresolve_hits_count) / total_preresolves;

  if (total_preconnects > 0) {
    size_t preconnect_hits_percentage =
        (100 * preconnect_hits_count) / total_preconnects;
    UMA_HISTOGRAM_PERCENTAGE(
        internal::kLoadingPredictorPreconnectHitsPercentage,
        preconnect_hits_percentage);
  }

  UMA_HISTOGRAM_PERCENTAGE(internal::kLoadingPredictorPreresolveHitsPercentage,
                           preresolve_hits_percentage);
  UMA_HISTOGRAM_COUNTS_100(internal::kLoadingPredictorPreresolveCount,
                           total_preresolves);
  UMA_HISTOGRAM_COUNTS_100(internal::kLoadingPredictorPreconnectCount,
                           total_preconnects);
}

}  // namespace

LoadingStatsCollector::LoadingStatsCollector(
    ResourcePrefetchPredictor* predictor,
    const LoadingPredictorConfig& config)
    : predictor_(predictor),
      max_stats_age_(base::Seconds(config.max_navigation_lifetime_seconds)) {}

LoadingStatsCollector::~LoadingStatsCollector() = default;

void LoadingStatsCollector::RecordPreconnectStats(
    std::unique_ptr<PreconnectStats> stats) {
  const GURL& main_frame_url = stats->url;
  auto it = preconnect_stats_.find(main_frame_url);
  if (it != preconnect_stats_.end()) {
    ReportPreconnectAccuracy(*it->second, {});
    preconnect_stats_.erase(it);
  }

  preconnect_stats_.emplace(main_frame_url, std::move(stats));
}

void LoadingStatsCollector::RecordPageRequestSummary(
    const PageRequestSummary& summary,
    const std::optional<OptimizationGuidePrediction>&
        optimization_guide_prediction) {
  if (!summary.main_frame_load_complete) {
    return;
  }

  const GURL& initial_url = summary.initial_url;

  ukm::builders::LoadingPredictor builder(summary.ukm_source_id);
  bool recorded_ukm = false;
  size_t ukm_cap = 100;

  PreconnectPrediction preconnect_prediction;
  if (predictor_->PredictPreconnectOrigins(initial_url,
                                           &preconnect_prediction)) {
    size_t correctly_predicted_origins = ReportPreconnectPredictionAccuracy(
        preconnect_prediction, summary, HintOrigin::NAVIGATION);
    if (!preconnect_prediction.requests.empty()) {
      builder.SetLocalPredictionOrigins(
          std::min(ukm_cap, preconnect_prediction.requests.size()));
      builder.SetLocalPredictionCorrectlyPredictedOrigins(
          std::min(ukm_cap, correctly_predicted_origins));
      recorded_ukm = true;
    }
  }
  if (optimization_guide_prediction) {
    builder.SetOptimizationGuidePredictionDecision(
        static_cast<int64_t>(optimization_guide_prediction->decision));
    if (optimization_guide_prediction->optimization_guide_prediction_arrived) {
      builder.SetNavigationStartToOptimizationGuidePredictionArrived(
          (optimization_guide_prediction->optimization_guide_prediction_arrived
               .value() -
           summary.navigation_started)
              .InMilliseconds());
    }
    if (!optimization_guide_prediction->preconnect_prediction.requests
             .empty()) {
      ReportPreconnectPredictionAccuracy(
          optimization_guide_prediction->preconnect_prediction, summary,
          HintOrigin::OPTIMIZATION_GUIDE);
    }
    if (!optimization_guide_prediction->predicted_subresources.empty()) {
      url::Origin main_frame_origin = url::Origin::Create(summary.initial_url);
      size_t cross_origin_predicted_subresources = 0;
      size_t correctly_predicted_subresources = 0;
      size_t correctly_predicted_cross_origin_subresources = 0;
      size_t correctly_predicted_low_priority_subresources = 0;
      size_t correctly_predicted_low_priority_cross_origin_subresources = 0;

      for (const GURL& subresource_url :
           optimization_guide_prediction->predicted_subresources) {
        const bool is_cross_origin =
            !main_frame_origin.IsSameOriginWith(subresource_url);
        const bool is_correctly_predicted =
            base::Contains(summary.subresource_urls, subresource_url);
        const bool is_correctly_predicted_low_priority =
            !is_correctly_predicted &&
            base::Contains(summary.low_priority_subresource_urls,
                           subresource_url);
        if (is_cross_origin) {
          cross_origin_predicted_subresources++;
        }
        if (is_correctly_predicted) {
          correctly_predicted_subresources++;
        }
        if (is_cross_origin && is_correctly_predicted) {
          correctly_predicted_cross_origin_subresources++;
        }
        if (is_correctly_predicted_low_priority) {
          correctly_predicted_low_priority_subresources++;
        }
        if (is_cross_origin && is_correctly_predicted_low_priority) {
          correctly_predicted_low_priority_cross_origin_subresources++;
        }
      }

      builder.SetOptimizationGuidePredictionSubresources(std::min(
          ukm_cap,
          optimization_guide_prediction->predicted_subresources.size()));
      builder.SetOptimizationGuidePredictionSubresources_CrossOrigin(
          std::min(ukm_cap, cross_origin_predicted_subresources));
      builder.SetOptimizationGuidePredictionCorrectlyPredictedSubresources(
          std::min(ukm_cap, correctly_predicted_subresources));
      builder
          .SetOptimizationGuidePredictionCorrectlyPredictedSubresources_CrossOrigin(
              std::min(ukm_cap, correctly_predicted_cross_origin_subresources));
      builder
          .SetOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresources(
              std::min(ukm_cap, correctly_predicted_low_priority_subresources));
      builder
          .SetOptimizationGuidePredictionCorrectlyPredictedLowPrioritySubresources_CrossOrigin(
              std::min(
                  ukm_cap,
                  correctly_predicted_low_priority_cross_origin_subresources));

      std::set<url::Origin> predicted_origins;
      for (const auto& subresource :
           optimization_guide_prediction->predicted_subresources) {
        url::Origin subresource_origin = url::Origin::Create(subresource);
        if (subresource_origin == main_frame_origin) {
          // Do not count the main frame origin as a predicted origin.
          continue;
        }
        predicted_origins.insert(subresource_origin);
      }
      builder.SetOptimizationGuidePredictionOrigins(
          std::min(ukm_cap, predicted_origins.size()));
      size_t correctly_predicted_origins = base::ranges::count_if(
          predicted_origins, [&summary](const url::Origin& subresource_origin) {
            return base::Contains(summary.origins, subresource_origin);
          });
      builder.SetOptimizationGuidePredictionCorrectlyPredictedOrigins(
          std::min(ukm_cap, correctly_predicted_origins));
      size_t correctly_predicted_low_priority_origins = base::ranges::count_if(
          predicted_origins, [&summary](const url::Origin& subresource_origin) {
            return base::Contains(summary.low_priority_origins,
                                  subresource_origin) &&
                   !base::Contains(summary.origins, subresource_origin);
          });
      builder
          .SetOptimizationGuidePredictionCorrectlyPredictedLowPriorityOrigins(
              std::min(ukm_cap, correctly_predicted_low_priority_origins));
    }
    recorded_ukm = true;
  }
  if (!summary.preconnect_origins.empty()) {
    builder.SetSubresourceOriginPreconnectsInitiated(
        std::min(ukm_cap, summary.preconnect_origins.size()));
    const auto& actual_subresource_origins = summary.origins;
    size_t correctly_predicted_subresource_origins_initiated =
        base::ranges::count_if(
            summary.preconnect_origins,
            [&actual_subresource_origins](
                const url::Origin& subresource_origin) {
              return actual_subresource_origins.find(subresource_origin) !=
                     actual_subresource_origins.end();
            });
    builder.SetCorrectSubresourceOriginPreconnectsInitiated(
        std::min(ukm_cap, correctly_predicted_subresource_origins_initiated));
    recorded_ukm = true;
  }
  if (!summary.prefetch_urls.empty()) {
    builder.SetSubresourcePrefetchesInitiated(
        std::min(ukm_cap, summary.prefetch_urls.size()));
    const auto& actual_subresource_urls = summary.subresource_urls;
    size_t correctly_predicted_subresource_prefetches_initiated =
        base::ranges::count_if(
            summary.prefetch_urls,
            [&actual_subresource_urls](const GURL& subresource_url) {
              return actual_subresource_urls.find(subresource_url) !=
                     actual_subresource_urls.end();
            });
    builder.SetCorrectSubresourcePrefetchesInitiated(std::min(
        ukm_cap, correctly_predicted_subresource_prefetches_initiated));
    recorded_ukm = true;
  }
  if (summary.first_prefetch_initiated) {
    DCHECK(!summary.prefetch_urls.empty());
    builder.SetNavigationStartToFirstSubresourcePrefetchInitiated(
        (summary.first_prefetch_initiated.value() - summary.navigation_started)
            .InMilliseconds());
    recorded_ukm = true;
  }

  auto it = preconnect_stats_.find(initial_url);
  if (it != preconnect_stats_.end()) {
    ReportPreconnectAccuracy(*it->second, summary.origins);
    preconnect_stats_.erase(it);
  }

  if (recorded_ukm) {
    // Only record nav start to commit if we had any predictions.
    if (summary.navigation_committed) {
      builder.SetNavigationStartToNavigationCommit(
          (summary.navigation_committed.value() - summary.navigation_started)
              .InMilliseconds());
    }
    builder.Record(ukm::UkmRecorder::Get());
  }
}

void LoadingStatsCollector::CleanupAbandonedStats() {
  base::TimeTicks time_now = base::TimeTicks::Now();
  for (auto it = preconnect_stats_.begin(); it != preconnect_stats_.end();) {
    if (time_now - it->second->start_time > max_stats_age_) {
      ReportPreconnectAccuracy(*it->second, {});
      it = preconnect_stats_.erase(it);
    } else {
      ++it;
    }
  }
}

}  // namespace predictors
