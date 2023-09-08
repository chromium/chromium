// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/loading_predictor_config.h"

#include "base/metrics/field_trial_params.h"
#include "chrome/browser/predictors/predictors_features.h"
#include "chrome/browser/preloading/preloading_prefs.h"
#include "chrome/browser/profiles/profile.h"
#include "third_party/blink/public/common/features.h"

namespace predictors {

bool IsLoadingPredictorEnabled(Profile* profile) {
  // Disabled for off-the-record. Policy choice, not a technical limitation.
  return profile && !profile->IsOffTheRecord();
}

bool IsPreconnectAllowed(Profile* profile) {
  // Checks that the preconnect is allowed by user settings.
  return profile && profile->GetPrefs() &&
         (prefetch::IsSomePreloadingEnabled(*profile->GetPrefs()) ==
          content::PreloadingEligibility::kEligible);
}

std::string GetStringNameForHintOrigin(HintOrigin hint_origin) {
  switch (hint_origin) {
    case HintOrigin::NAVIGATION:
      return "Navigation";
    case HintOrigin::OPTIMIZATION_GUIDE:
      return "OptimizationGuide";
    default:
      NOTREACHED();
      return "";
  }
}

LoadingPredictorConfig::LoadingPredictorConfig()
    : max_navigation_lifetime_seconds(base::GetFieldTrialParamByFeatureAsInt(
          features::kLoadingPredictorTableConfig,
          "max_navigation_lifetime_seconds",
          60)),
      max_hosts_to_track(base::GetFieldTrialParamByFeatureAsInt(
          features::kLoadingPredictorTableConfig,
          "max_hosts_to_track",
          100)),
      max_hosts_to_track_for_lcpp(base::GetFieldTrialParamByFeatureAsInt(
          blink::features::kLCPCriticalPathPredictor,
          "lcpp_max_hosts_to_track",
          100)),
      max_origins_per_entry(base::GetFieldTrialParamByFeatureAsInt(
          features::kLoadingPredictorTableConfig,
          "max_origins_per_entry",
          50)),
      max_consecutive_misses(3),
      max_redirect_consecutive_misses(5),
      flush_data_to_disk_delay_seconds(30),
      lcpp_histogram_sliding_window_size(base::GetFieldTrialParamByFeatureAsInt(
          features::kLoadingPredictorTableConfig,
          "lcpp_histogram_sliding_window_size",
          1000)),
      max_lcpp_histogram_buckets(base::GetFieldTrialParamByFeatureAsInt(
          features::kLoadingPredictorTableConfig,
          "max_lcpp_histogram_buckets",
          10)) {}

LoadingPredictorConfig::LoadingPredictorConfig(
    const LoadingPredictorConfig& other) = default;

LoadingPredictorConfig::~LoadingPredictorConfig() = default;

}  // namespace predictors
