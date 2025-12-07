// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_stats_collector.h"

#include <algorithm>
#include <set>
#include <vector>

#include "base/containers/contains.h"
#include "chrome/browser/predictors/loading_data_collector.h"
#include "chrome/browser/predictors/resource_prefetch_predictor.h"
#include "content/public/browser/preconnect_request.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "services/metrics/public/cpp/ukm_source.h"

namespace predictors {

namespace {

using RedirectStatus = ResourcePrefetchPredictor::RedirectStatus;

// Returns the number of origins that were correctly predicted.
size_t CountCorrectlyPredictiedOrigins(const PreconnectPrediction& prediction,
                                       const PageRequestSummary& summary,
                                       HintOrigin hint_origin) {
  if (prediction.requests.empty() || summary.origins.empty()) {
    return 0;
  }

  const auto& actual_origins = summary.origins;

  size_t correctly_predicted_count = std::ranges::count_if(
      prediction.requests,
      [&actual_origins](const content::PreconnectRequest& request) {
        return actual_origins.find(request.origin) != actual_origins.end();
      });

  return correctly_predicted_count;
}

}  // namespace

LoadingStatsCollector::LoadingStatsCollector(
    ResourcePrefetchPredictor* predictor)
    : predictor_(predictor) {}

LoadingStatsCollector::~LoadingStatsCollector() = default;

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
    size_t correctly_predicted_origins = CountCorrectlyPredictiedOrigins(
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
      size_t correctly_predicted_origins = std::ranges::count_if(
          predicted_origins, [&summary](const url::Origin& subresource_origin) {
            return base::Contains(summary.origins, subresource_origin);
          });
      builder.SetOptimizationGuidePredictionCorrectlyPredictedOrigins(
          std::min(ukm_cap, correctly_predicted_origins));
      size_t correctly_predicted_low_priority_origins = std::ranges::count_if(
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
        std::ranges::count_if(
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
        std::ranges::count_if(
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

}  // namespace predictors
