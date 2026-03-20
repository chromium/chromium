// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FINDS_CORE_FINDS_FEATURES_H_
#define CHROME_BROWSER_FINDS_CORE_FINDS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace finds::features {

// The feature flag that enables the Finds surface on Android.
BASE_DECLARE_FEATURE(kChromeFinds);

// The feature flag that enables the chrome://finds-internals page.
BASE_DECLARE_FEATURE(kChromeFindsInternals);

// The cooldown period in days for the model execution cooldown.
extern const base::FeatureParam<int> kModelExecutionCooldownDurationInDays;

// The cooldown period in days for each theme not interested.
extern const base::FeatureParam<int> kThemeCooldownDurationInDays;

}  // namespace finds::features

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_FEATURES_H_
