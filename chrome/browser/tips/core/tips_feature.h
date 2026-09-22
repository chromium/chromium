// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TIPS_CORE_TIPS_FEATURE_H_
#define CHROME_BROWSER_TIPS_CORE_TIPS_FEATURE_H_

#include <map>
#include <string>
#include <vector>

#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"
#include "chrome/browser/notifications/scheduler/public/notification_params.h"
#include "chrome/browser/notifications/scheduler/public/notification_scheduler_types.h"
#include "chrome/browser/tips/core/tips_types.h"

#if BUILDFLAG(IS_ANDROID)
#include "chrome/browser/flags/android/chrome_feature_list.h"
#endif

class PrefService;

namespace tips {

#if BUILDFLAG(IS_ANDROID)
// Parameter to trigger instant notification delivery and bypass the global
// 7-day cooldown for testing.
inline constexpr base::FeatureParam<bool> kTipsSelfServiceInstantScheduling(
    &chrome::android::kTipsSelfService,
    "instant_scheduling",
    /*default_value=*/false);

// The delay in minutes after which the notification scheduler should start
// delivering the chosen tip.
inline constexpr base::FeatureParam<int> kTipsSelfServiceStartTimeMinutes(
    &chrome::android::kTipsSelfService,
    "start_time_minutes",
    /*default_value=*/120);

// The delivery window duration in minutes during which the notification
// scheduler can deliver the notification.
inline constexpr base::FeatureParam<int> kTipsSelfServiceWindowTimeMinutes(
    &chrome::android::kTipsSelfService,
    "window_time_minutes",
    /*default_value=*/120);
#endif  // BUILDFLAG(IS_ANDROID)

// Abstract interface that individual feature teams must implement to register a
// new Tips notification.
class TipsFeature {
 public:
  virtual ~TipsFeature() = default;

  // Returns the ranking priority of this tip.
  virtual TipFeatureRank GetRank() const = 0;

  // Returns the corresponding TipsNotificationsFeatureType enum value.
  virtual TipsNotificationsFeatureType GetFeatureType() const = 0;

  // Returns the list of database signals this feature needs to query to
  // evaluate eligibility.
  virtual std::vector<SignalDefinition> GetRequiredSignals() const = 0;

  // Checks if the feature is eligible to be shown based on the queried signal
  // values and user preferences. `signal_values` maps the signal name to its
  // aggregated value (e.g., sum or count). `pref_service` provides access to
  // the active user profile's preferences.
  virtual bool IsEligible(const std::map<std::string, float>& signal_values,
                          const PrefService& pref_service) const = 0;

  // Returns the visual notification parameters to display if this tip is
  // selected.
  virtual notifications::NotificationData GetNotificationData() const = 0;
};

}  // namespace tips

#endif  // CHROME_BROWSER_TIPS_CORE_TIPS_FEATURE_H_
