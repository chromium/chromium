// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
#define CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.

extern const base::Feature kLoadingPredictorUseLocalPredictions;

extern const base::Feature kLoadingOnlyLearnHighPriorityResources;

extern const base::Feature kLoadingPredictorTableConfig;

extern const base::Feature kLoadingPreconnectToRedirectTarget;

extern const base::Feature kLoadingPredictorDisregardAlwaysAccessesNetwork;

extern const base::Feature kLoadingPredictorUseOptimizationGuide;

extern const base::Feature kLoadingPredictorPrefetch;

enum class PrefetchSubresourceType { kAll, kCss, kJsAndCss };

extern const base::FeatureParam<PrefetchSubresourceType>
    kLoadingPredictorPrefetchSubresourceType;

extern const base::Feature kLoadingPredictorInflightPredictiveActions;

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

// Returns the maximum number of preresolves that can be inflight at any given
// time.
size_t GetMaxInflightPreresolves();

// Returns the maximum number of prefetches that can be inflight at any given
// time.
size_t GetMaxInflightPrefetches();

}  // namespace features

#endif  // CHROME_BROWSER_PREDICTORS_PREDICTORS_FEATURES_H_
