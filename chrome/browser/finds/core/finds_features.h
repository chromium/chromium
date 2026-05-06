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

// The start time in minutes for scheduling a finds notification. The start time
// is in relation to base::Time::Now() when the intent to schedule a
// notification is serviced. For example, if the start time is 2 that means the
// notification will be scheduled to show after 2 minutes have passed from now.
// This value works in conjunction with kNotificationWindowTimeMinutes.
extern const base::FeatureParam<int> kNotificationStartTimeMinutes;

// The window time in minutes for the elapsed time since the start time. The
// window time is in relation to base::Time::Now() in addition to the start
// time, when the intent to schedule a notification is serviced. This is needed
// because the background task scheduler is not guaranteed run tasks scheduled
// at a specific time and requires a window period to run tasks in the future.
// For example, if the window time is 2 and start time is 2 that means the
// notification will be scheduled to show between 2 min (start time) and 4 min
// (start+window time) from now. This is tied to kNotificationStartTimeMinutes.
extern const base::FeatureParam<int> kNotificationWindowTimeMinutes;

// The maximum number of times the user needs to interact with the promo before
// they are no longer prompted.
extern const base::FeatureParam<int> kFindsOptInPromoMaxInteractedCount;

// The amount of time in days that must pass since the last promo interaction
// before the user can be prompted again.
extern const base::FeatureParam<int> kFindsOptInPromoCooldownInDays;

// The number of times a theme should be visited in order for a user to be
// eligible to receive the opt in promo for finds.
extern const base::FeatureParam<int> kThemeUrlVisitCountForOptIn;

// The maximum upper limit for history entries to be included in the LLM query.
// A value of 0 means "no limit".
extern const base::FeatureParam<int> kMaxHistoryEntries;

// The number of times a user should return to the SRP before we consider
// triggering the opt in.
extern const base::FeatureParam<int> kSRPReturnCountThreshold;

// The feature flag param to enable the SRP return count opt-in flow.
extern const base::FeatureParam<bool> kEnableSrpReturnCountOptIn;

// The feature flag param to enable the theme URL visit count opt-in flow.
extern const base::FeatureParam<bool> kEnableThemeUrlVisitCountOptIn;

// The feature flag param to block model execution.
extern const base::FeatureParam<bool> kBlockModelExecution;

}  // namespace finds::features

#endif  // CHROME_BROWSER_FINDS_CORE_FINDS_FEATURES_H_
