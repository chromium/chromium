// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tpcd/experiment/tpcd_experiment_features.h"

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace tpcd::experiment {

// Enables the Third Party Cookie Deprecation (TPCD) Mode B Experiment
// feature.
BASE_FEATURE(k3PCDModeBExperiment,
             "3PCDModeBExperiment",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Set which experiment cohort a user is assigned to ("modeb", "modebprime",
// "control", etc.).
const base::FeatureParam<std::string> kCohort{
    &k3PCDModeBExperiment, /*name=*/"cohort", /*default_value=*/""};

const base::FeatureParam<bool> kDisable3PCookies{&k3PCDModeBExperiment,
                                                 /*name=*/"disable_3p_cookies",
                                                 /*default_value=*/false};

const base::FeatureParam<bool> kDisableAdsAPIs{&k3PCDModeBExperiment,
                                               /*name=*/"disable_ads_apis",
                                               /*default_value=*/false};

const base::FeatureParam<bool> kLabelTraffic{
    &k3PCDModeBExperiment, /*name=*/"label_traffic", /*default_value=*/false};

}  // namespace tpcd::experiment
