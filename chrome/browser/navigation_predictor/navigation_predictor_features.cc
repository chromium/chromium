// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/navigation_predictor/navigation_predictor_features.h"

#include "build/build_config.h"

namespace features {

// A holdback that prevents the preconnect to measure benefit of the feature.
const base::Feature kNavigationPredictorPreconnectHoldback {
  "NavigationPredictorPreconnectHoldback",
#if defined(OS_ANDROID)
      base::FEATURE_DISABLED_BY_DEFAULT
#else
      base::FEATURE_ENABLED_BY_DEFAULT
#endif
};

// Enables triggering of same-origin preconnects on same-document navigations.
const base::Feature
    kNavigationPredictorEnablePreconnectOnSameDocumentNavigations{
        "NavigationPredictorEnablePreconnectOnSameDocumentNavigations",
        base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
