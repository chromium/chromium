// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictors_features.h"

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace features {

namespace {

constexpr base::FeatureState kFeatureEnabledOnlyOnAndroid =
    BUILDFLAG(IS_ANDROID) ? base::FEATURE_ENABLED_BY_DEFAULT
                          : base::FEATURE_DISABLED_BY_DEFAULT;

}  // namespace

// Whether local predictions should be used to make preconnect predictions.
BASE_FEATURE(kLoadingPredictorUseLocalPredictions,
             "LoadingPredictorUseLocalPredictions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Modifies loading predictor so that it only learns about subresources and
// origins that are high priority.
BASE_FEATURE(kLoadingOnlyLearnHighPriorityResources,
             "LoadingOnlyLearnHighPriorityResources",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Configures the loading predictor table size and other base parameters.
BASE_FEATURE(kLoadingPredictorTableConfig,
             "LoadingPredictorTableConfig",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Modifies loading predictor so that the predictions also contain origins of
// the redirect target of the navigation.
BASE_FEATURE(kLoadingPreconnectToRedirectTarget,
             "LoadingPreconnectToRedirectTarget",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Modifies loading predictor so that the value of the |always_access_network|
// attribute is not used when computing the predicting score for an origin.
BASE_FEATURE(kLoadingPredictorDisregardAlwaysAccessesNetwork,
             "LoadingPredictorDisregardAlwaysAccessesNetwork",
             base::FEATURE_DISABLED_BY_DEFAULT);

constexpr base::FeatureState
    kLoadingPredictorUseOptimizationGuideDefaultFeatureState =
        kFeatureEnabledOnlyOnAndroid;

// Modifies loading predictor so that it can also use predictions coming from
// the optimization guide.
BASE_FEATURE(kLoadingPredictorUseOptimizationGuide,
             "LoadingPredictorUseOptimizationGuide",
             kLoadingPredictorUseOptimizationGuideDefaultFeatureState);

constexpr base::FeatureState kLoadingPredictorPrefetchDefaultFeatureState =
    kFeatureEnabledOnlyOnAndroid;

// Modifies loading predictor so that it does prefetches of subresources instead
// of preconnects.
BASE_FEATURE(kLoadingPredictorPrefetch,
             "LoadingPredictorPrefetch",
             kLoadingPredictorPrefetchDefaultFeatureState);

// Use the kURLLoadOptionReadAndDiscardBody option to URLLoader to avoid
// unnecessarily copying response body data.
BASE_FEATURE(kLoadingPredictorPrefetchUseReadAndDiscardBody,
             "LoadingPredictorPrefetchUseReadAndDiscardBody",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<PrefetchSubresourceType>::Option
    kPrefetchSubresourceTypeParamOptions[] = {
        {PrefetchSubresourceType::kAll, "all"},
        {PrefetchSubresourceType::kCss, "css"},
        {PrefetchSubresourceType::kJsAndCss, "js_css"}};

const base::FeatureParam<PrefetchSubresourceType>
    kLoadingPredictorPrefetchSubresourceType{
        &kLoadingPredictorPrefetch, "subresource_type",
        PrefetchSubresourceType::kAll, &kPrefetchSubresourceTypeParamOptions};

bool ShouldUseLocalPredictions() {
  return base::FeatureList::IsEnabled(kLoadingPredictorUseLocalPredictions);
}

bool ShouldUseOptimizationGuidePredictions() {
  if (!base::FeatureList::IsEnabled(kLoadingPredictorUseOptimizationGuide))
    return false;

  return base::GetFieldTrialParamByFeatureAsBool(
      kLoadingPredictorUseOptimizationGuide, "use_predictions", true);
}

bool ShouldAlwaysRetrieveOptimizationGuidePredictions() {
  return base::GetFieldTrialParamByFeatureAsBool(
      kLoadingPredictorUseOptimizationGuide, "always_retrieve_predictions",
      false);
}

// If this is enabled, LoadingPredictor avoids prefetching during
// browser startup.
BASE_FEATURE(kAvoidLoadingPredictorPrefetchDuringBrowserStartup,
             "AvoidLoadingPredictorPrefetchDuringBrowserStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If this is enabled, LoadingPredictor restricts the number of preconnects for
// the same destination to one.
BASE_FEATURE(kLoadingPredictorLimitPreconnectSocketCount,
             "LoadingPredictorLimitPreconnectSocketCount",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
