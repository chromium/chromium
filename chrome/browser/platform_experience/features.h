// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PLATFORM_EXPERIENCE_FEATURES_H_
#define CHROME_BROWSER_PLATFORM_EXPERIENCE_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace platform_experience::features {

// Allows Chrome to load PEH features gated on "low engagement" into
// preferences, for later use by PEH.
BASE_DECLARE_FEATURE(kLoadLowEngagementPEHFeaturesToPrefs);

// Forces the PEH to never show notifications.
BASE_DECLARE_FEATURE(kDisablePEHNotifications);

// By default, a random notification text is chosen when a notification shows.
// This feature allows for a specific notification text to be used.
// kUseNotificationTextIndex specifies which text will be used if a notification
// will show.
BASE_DECLARE_FEATURE(kShouldUseSpecificPEHNotificationText);

// Defines which notification text will be used if
// kShouldUseSpecificNotificationText is enabled.
inline constexpr base::FeatureParam<int> kUseNotificationTextIndex{
    &kShouldUseSpecificPEHNotificationText, "UseNotificationTextIndex", 0};

// This may block.
// Reflects field trial activations from the PEH by forcing them to activate
// in Chrome.
void ActivateFieldTrials();

}  // namespace platform_experience::features

#endif  // CHROME_BROWSER_PLATFORM_EXPERIENCE_FEATURES_H_
