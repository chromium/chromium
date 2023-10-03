// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_
#define CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_

#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"

namespace tpcd::experiment {

extern const char kVersionName[];
extern const char kDisable3PCookiesName[];
extern const char kDecisionDelayTimeName[];
extern const char kForceEligibleForTestingName[];

extern const base::FeatureParam<int> kVersion;
extern const base::FeatureParam<bool> kDisable3PCookies;
extern const base::FeatureParam<base::TimeDelta> kDecisionDelayTime;
extern const base::FeatureParam<bool> kForceEligibleForTesting;

}  // namespace tpcd::experiment

#endif  // CHROME_BROWSER_TPCD_EXPERIMENT_TPCD_EXPERIMENT_FEATURES_H_
