// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include <string>

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

// Set the version of the experiment finch config.
const base::FeatureParam<int> kVersion{
    &features::kCookieDeprecationFacilitatedTesting, /*name=*/"version",
    /*default_value=*/0};

// True IFF third-party cookies are disabled.
// Distinguishes between "Mode A" and "Mode B" cohorts.
const base::FeatureParam<bool> kDisable3PCookies{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"disable_3p_cookies",
    /*default_value=*/false};

// Whether Ads APIs should be disabled.
const base::FeatureParam<bool> kDisableAdsAPIs{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"disable_ads_apis",
    /*default_value=*/false};

extern const base::FeatureParam<base::TimeDelta> kDecisionDelayTime{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"decision_delay_time",
    /*default_value=*/base::Seconds(1)};

}  // namespace tpcd::experiment
