// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_FEATURES_H_
#define CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace omnibox::mojom {
enum class NavigationPredictor;
}

namespace features {

// DefaultSearchEngine Preload v2
// https://crbug.com/394988793
BASE_DECLARE_FEATURE(kDsePreload2);

extern const base::FeatureParam<bool> kDsePreload2PredictorMouseDown;
extern const base::FeatureParam<bool> kDsePreload2PredictorUpOrDownArrowButton;
extern const base::FeatureParam<bool> kDsePreload2PredictorTouchDown;

// Returns true iff we should enter DsePreload2 code path.
bool IsDsePreload2Enabled();
// Returns true iff we should start prefetch on
// `SearchPreloadService::OnNavigationLikely()` with a predictor.
bool DsePreload2IsPredictorEnabled(
    omnibox::mojom::NavigationPredictor navigation_predictor);

}  // namespace features

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_FEATURES_H_
