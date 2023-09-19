// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

// Set which experiment cohort a user is assigned to ("modea", "modeb",
// "modebprime", "control", etc.).
const base::FeatureParam<std::string> kCohort{
    &features::kCookieDeprecationFacilitatedTesting, /*name=*/"cohort",
    /*default_value=*/""};

const base::FeatureParam<bool> kDisable3PCookies{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"disable_3p_cookies",
    /*default_value=*/false};

const base::FeatureParam<bool> kDisableAdsAPIs{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"disable_ads_apis",
    /*default_value=*/false};

}  // namespace tpcd::experiment
