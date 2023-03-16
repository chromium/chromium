// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eol_incentive_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"

namespace ash::eol_incentive_util {
namespace {

// The minimum number of days that the current user profile must have existed on
// the current device.
constexpr int kMinimumUseTimeInDays = 180;

// The number of days before EOL, after which the EOL incentive is shown.
constexpr int kFirstIncentiveDaysInAdvance = 30;

// The number of days past the EOL within which the last incentive notification
// is shown.
constexpr int kLastIncentiveEndDaysPastEol = -5;

}  // namespace

EolIncentiveType ShouldShowEolIncentive(Profile* profile,
                                        base::Time eol_date,
                                        base::Time now) {
  if (!features::IsEOLIncentiveEnabled()) {
    return kNone;
  }

  if (eol_date.is_null()) {
    return kNone;
  }

  // Do not show end of life notification if this device is managed by
  // enterprise user.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    return kNone;
  }

  const base::Time creation_time =
      profile->GetPrefs()->GetTime(prefs::kProfileCreationTime);
  const base::TimeDelta time_since_creation = now - creation_time;

  // Only show the incentive for a user that has used the device longer than the
  // minimum time required.
  if (time_since_creation.InDays() < kMinimumUseTimeInDays &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEolIgnoreProfileCreationTime)) {
    return kNone;
  }

  const base::TimeDelta time_to_eol = eol_date - now;
  const int days_to_eol = time_to_eol.InDays();

  // Show the EOL approaching incentive when `days_to_eol` is more than zero
  // but less than `kFirstIncentiveDaysInAdvance` days away.
  if (days_to_eol > 0 && days_to_eol <= kFirstIncentiveDaysInAdvance) {
    return kEolApproaching;
  }

  // Show the EOL passed incentive when on the EOL date or within
  // `kLastIncentiveEndDaysPastEol` days after the EOL date has passed.
  if (days_to_eol <= 0 && days_to_eol >= kLastIncentiveEndDaysPastEol) {
    return kEolPassed;
  }

  return kNone;
}

}  // namespace ash::eol_incentive_util
