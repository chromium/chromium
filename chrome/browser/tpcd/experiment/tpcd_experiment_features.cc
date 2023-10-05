// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "content/public/common/content_features.h"

namespace tpcd::experiment {

const char kVersionName[] = "version";
const char kDisable3PCookiesName[] = "disable_3p_cookies";
const char kForceEligibleForTestingName[] = "force_eligible";

// Set the version of the experiment finch config.
const base::FeatureParam<int> kVersion{
    &features::kCookieDeprecationFacilitatedTesting, /*name=*/kVersionName,
    /*default_value=*/0};

// True IFF third-party cookies are disabled.
// Distinguishes between "Mode A" and "Mode B" cohorts.
const base::FeatureParam<bool> kDisable3PCookies{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kDisable3PCookiesName,
    /*default_value=*/false};

extern const base::FeatureParam<base::TimeDelta> kDecisionDelayTime{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/"decision_delay_time",
    /*default_value=*/base::Seconds(1)};

// Set whether to force client being eligible for manual testing.
const base::FeatureParam<bool> kForceEligibleForTesting{
    &features::kCookieDeprecationFacilitatedTesting,
    /*name=*/kForceEligibleForTestingName,
    /*default_value=*/false};

}  // namespace tpcd::experiment
