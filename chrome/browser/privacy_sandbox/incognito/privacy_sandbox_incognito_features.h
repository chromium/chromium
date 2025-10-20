// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/component_export.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"

#ifndef CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_
#define CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_

namespace privacy_sandbox {

// Show ACT features on the What's New page.
BASE_DECLARE_FEATURE(kPrivacySandboxActWhatsNew);

// Privacy Sandbox ACT What's New survey. Displayed on a NTP provided the user
// has seen the What's New with ACT release notes.
BASE_DECLARE_FEATURE(kPrivacySandboxWhatsNewSurvey);

extern const base::FeatureParam<base::TimeDelta>
    kPrivacySandboxWhatsNewSurveyDelay;

}  // namespace privacy_sandbox

#endif  // CHROME_BROWSER_PRIVACY_SANDBOX_INCOGNITO_PRIVACY_SANDBOX_INCOGNITO_FEATURES_H_
