// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

BASE_DECLARE_FEATURE(kLoadingPredictorUseLocalPredictions);

BASE_DECLARE_FEATURE(kLoadingOnlyLearnHighPriorityResources);

BASE_DECLARE_FEATURE(kLoadingPredictorTableConfig);

BASE_DECLARE_FEATURE(kLoadingPreconnectToRedirectTarget);

BASE_DECLARE_FEATURE(kLoadingPredictorDisregardAlwaysAccessesNetwork);

BASE_DECLARE_FEATURE(kLoadingPredictorUseOptimizationGuide);

BASE_DECLARE_FEATURE(kLoadingPredictorPrefetch);

enum class PrefetchSubresourceType { kAll, kCss, kJsAndCss };

extern const base::FeatureParam<PrefetchSubresourceType>
    kLoadingPredictorPrefetchSubresourceType;

BASE_DECLARE_FEATURE(kLoadingPredictorPrefetchUseReadAndDiscardBody);

// Returns whether local predictions should be used to make preconnect
// predictions.
bool ShouldUseLocalPredictions();

// Returns whether optimization guide predictions should be used to make
// loading predictions, such as preconnect or prefetch.
//
// In addition to checking whether the feature is enabled, this will
// additionally check a feature parameter is specified to dictate if the
// predictions should actually be used.
bool ShouldUseOptimizationGuidePredictions();

// Returns whether optimization guide predictions should always be retrieved,
// even if local predictions are available for preconnect predictions.
bool ShouldAlwaysRetrieveOptimizationGuidePredictions();

BASE_DECLARE_FEATURE(kAvoidLoadingPredictorPrefetchDuringBrowserStartup);

BASE_DECLARE_FEATURE(kLoadingPredictorLimitPreconnectSocketCount);

}  // namespace features

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
