// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"

#include "build/build_config.h"

namespace features {

// A holdback that prevents the preconnect to measure benefit of the feature.
BASE_FEATURE(kNavigationPredictorPreconnectHoldback,
             "NavigationPredictorPreconnectHoldback",
#if BUILDFLAG(IS_ANDROID)
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

}  // namespace features
