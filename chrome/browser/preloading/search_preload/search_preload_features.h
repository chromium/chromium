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
//
// - Base feature flag for DSE preload 2.
// - Disables `SearchPrefetch` (DSE v1).
// - Enables on-suggest preloads.
//
// https://crbug.com/394988793
BASE_DECLARE_FEATURE(kDsePreload2);

// Enable `PreloadServingMetrics`.
extern const base::FeatureParam<bool> kDsePreload2UsePreloadServingMetrics;

// The feature is disabled if device memory is smaller than the threshold.
extern const base::FeatureParam<size_t> kDsePreload2DeviceMemoryThresholdMiB;
// Pause triggering preloads when on-suggest prefetch failed.
extern const base::FeatureParam<base::TimeDelta>
    kDsePreload2ErrorBackoffDuration;
extern const base::FeatureParam<size_t> kDsePreload2MaxPrefetch;
// Time to live (TTL) of prefetch.
extern const base::FeatureParam<base::TimeDelta> kDsePreload2PrefetchTtl;

// Enables on-press prefetch.
BASE_DECLARE_FEATURE(kDsePreload2OnPress);

extern const base::FeatureParam<bool> kDsePreload2OnPressMouseDown;
extern const base::FeatureParam<bool> kDsePreload2OnPressUpOrDownArrowButton;
extern const base::FeatureParam<bool> kDsePreload2OnPressTouchDown;

// Enables on-press trigger in incognito mode.
//
// Corresponds to `SearchNavigationPrefetch.allow_incognito`.
//
// For more details, see
// https://docs.google.com/document/d/1f4dcNYP3O_Ft4yMmC42ETxGC5lM7YF5FDbEgnxUua7M/edit?tab=t.38v8gca76tmi
BASE_DECLARE_FEATURE(kDsePreload2OnPressIncognito);

// Enables on-suggest prefetch for non default match.
//
// For more details, see
// https://docs.google.com/document/d/1f4dcNYP3O_Ft4yMmC42ETxGC5lM7YF5FDbEgnxUua7M/edit?tab=t.38v8gca76tmi
BASE_DECLARE_FEATURE(kDsePreload2OnSuggestNonDefalutMatch);

// Returns true iff we should enter DsePreload2 code path.
bool IsDsePreload2Enabled();

// Returns true iff on-press triggers are enabled.
bool IsDsePreload2OnPressEnabled();
// Returns true iff we should start prefetch on
// `SearchPreloadService::OnNavigationLikely()` with a predictor.
bool DsePreload2OnPressIsPredictorEnabled(
    omnibox::mojom::NavigationPredictor navigation_predictor);

// Returns true iff on-press in incognito is enabled.
bool IsDsePreload2OnPressIncognitoEnabled();

}  // namespace features

#endif  // CHROME_BROWSER_PRELOADING_SEARCH_PRELOAD_SEARCH_PRELOAD_FEATURES_H_
