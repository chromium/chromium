// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/eol/eol_incentive_util.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/browser_context_helper/browser_context_helper.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"

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
  if (profile->IsOffTheRecord()) {
    return EolIncentiveType::kNone;
  }

  if (eol_date.is_null()) {
    return EolIncentiveType::kNone;
  }

  // Do not show end of life notification if this device is managed by
  // enterprise user.
  if (g_browser_process->platform_part()
          ->browser_policy_connector_ash()
          ->IsDeviceEnterpriseManaged()) {
    return EolIncentiveType::kNone;
  }

  const base::Time creation_time = profile->GetCreationTime();
  const base::TimeDelta time_since_creation = now - creation_time;

  // Only show the incentive for a user that has used the device longer than the
  // minimum time required.
  if (time_since_creation.InDays() < kMinimumUseTimeInDays &&
      !base::CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kEolIgnoreProfileCreationTime)) {
    return EolIncentiveType::kNone;
  }

  const user_manager::User* user =
      BrowserContextHelper::Get()->GetUserByBrowserContext(profile);
  if (user && user->GetType() != user_manager::UserType::kRegular) {
    return EolIncentiveType::kNone;
  }

  // If EOL is more than kFirstIncentiveDaysInAdvance away, don't show any
  // incentives.
  const base::TimeDelta time_to_eol = eol_date - now;
  if (time_to_eol > base::Days(kFirstIncentiveDaysInAdvance)) {
    return EolIncentiveType::kNone;
  }

  if (!features::IsEOLIncentiveEnabled()) {
    return EolIncentiveType::kNone;
  }

  // Show the EOL approaching incentive when `time_to_eol` is more than zero
  // but less than `kFirstIncentiveDaysInAdvance` days away.
  if (time_to_eol > base::TimeDelta()) {
    return EolIncentiveType::kEolApproaching;
  }

  // EOL passed "recently" on the EOL date or within
  // `kLastIncentiveEndDaysPastEol` days after the EOL date.
  if (time_to_eol <= base::TimeDelta() &&
      time_to_eol > base::Days(kLastIncentiveEndDaysPastEol)) {
    return EolIncentiveType::kEolPassedRecently;
  }

  // Eol passed, but more than a few days ago.
  return EolIncentiveType::kEolPassed;
}

void RecordShowSourceHistogram(EolIncentiveShowSource source) {
  base::UmaHistogramEnumeration(kEolIncentiveShowSourceHistogramName, source);
}

void RecordButtonClicked(EolIncentiveButtonType type) {
  base::UmaHistogramEnumeration(kEolIncentiveURLButtonClicked, type);
}

}  // namespace ash::eol_incentive_util
