// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/predictors/predictors_features.h"

namespace features {

// Modifies loading predictor so that it only learns about subresources and
// origins that are high priority.
const base::Feature kLoadingOnlyLearnHighPriorityResources{
    "LoadingOnlyLearnHighPriorityResources", base::FEATURE_ENABLED_BY_DEFAULT};

// Modifies loading predictor so that the predictions also contain origins of
// the redirect target of the navigation.
const base::Feature kLoadingPreconnectToRedirectTarget{
    "LoadingPreconnectToRedirectTarget", base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
