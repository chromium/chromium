// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_

namespace privacy_sandbox {

// Privacy Sandbox ACT survey. Displayed on a NTP in Incognito.
BASE_DECLARE_FEATURE(kPrivacySandboxActSurvey);

// Delay of the survey. Used only when delay randomization is disabled.
extern const base::FeatureParam<base::TimeDelta> kPrivacySandboxActSurveyDelay;

// When set to true, the value of `delay` will be ignored and the delay wll be
// instead randomized uniformly from the interval [`delay_min`;`delay_max`].
extern const base::FeatureParam<bool> kPrivacySandboxActSurveyDelayRandomize;

// The minimum value of the randomized delay (inclusive)
extern const base::FeatureParam<base::TimeDelta>
    kPrivacySandboxActSurveyDelayMin;

// The maximum value of the randomized delay (inclusive)
extern const base::FeatureParam<base::TimeDelta>
    kPrivacySandboxActSurveyDelayMax;

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_
