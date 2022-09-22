// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_

#include "base/feature_list.h"

namespace features {

// All features in alphabetical order. The features should be documented
// alongside the definition of their values in the .cc file.
BASE_DECLARE_FEATURE(kNavigationPredictorPreconnectHoldback);
BASE_DECLARE_FEATURE(
    kNavigationPredictorEnablePreconnectOnSameDocumentNavigations);

}  // namespace features

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_FEATURES_H_
