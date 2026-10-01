// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"

#include "build/build_config.h"

namespace features {

// This feature enables preemptive refreshes for device bound sessions
// registered in the same site as the default search engine when the user is not
// actively navigating.
BASE_FEATURE(kDeviceBoundSessionsDsePrewarmer, base::FEATURE_DISABLED_BY_DEFAULT);

// See `DsePrewarmerLifetime` for the description of the modes.
constexpr base::FeatureParam<DsePrewarmerLifetime>::Option
    kDeviceBoundSessionsDsePrewarmerModeOptions[] = {
        {DsePrewarmerLifetime::kFollowPreconnector, "follow-preconnector"},
        {DsePrewarmerLifetime::kStickyRespectingPrefs,
         "sticky-respecting-prefs"},
        {DsePrewarmerLifetime::kSticky, "sticky"},
};
BASE_FEATURE_ENUM_PARAM(DsePrewarmerLifetime,
                        kDeviceBoundSessionsDsePrewarmerMode,
                        &kDeviceBoundSessionsDsePrewarmer,
                        DsePrewarmerLifetime::kFollowPreconnector,
                        &kDeviceBoundSessionsDsePrewarmerModeOptions);

// A holdback that prevents the preconnect to measure benefit of the feature.
BASE_FEATURE(kNavigationPredictorPreconnectHoldback,
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

}  // namespace features
