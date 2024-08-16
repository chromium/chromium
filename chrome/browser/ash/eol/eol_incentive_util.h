// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EOL_EOL_INCENTIVE_UTIL_H_
#define CHROME_BROWSER_ASH_EOL_EOL_INCENTIVE_UTIL_H_

class Profile;

namespace base {
class Time;
}  // namespace base

namespace ash::eol_incentive_util {

// The type of end of life (EOL) incentive to be shown.
enum class EolIncentiveType {
  // No EOL incentive is shown.
  kNone,

  // The case where the EOL incentive is shown because of an approaching EOL
  // date.
  kEolApproaching,

  // The case where the EOL incentive is shown because of a recently passed EOL
  // date.
  kEolPassedRecently,

  // EOL passed, more than a few days ago.
  kEolPassed,
};

// Returns whether the EOL incentive should be shown to the current user based
// on the eol date and current time.
// Checks the following:
// * EOL incentive feature is enabled.
// * Current user has existed for a minimum time and is not managed.
// * Current date is within certain time frame close to EOL date.
// * EOL date is not null.
EolIncentiveType ShouldShowEolIncentive(Profile* profile,
                                        base::Time eol_date,
                                        base::Time now);

// This enum is used to record UMA histogram values and should not be
// reordered. Please keep in sync with 'EolIncentiveShowSource' in
// src/tools/metrics/histograms/enums.xml.
enum class EolIncentiveShowSource {
  kNotification_Approaching = 0,
  kNotification_RecentlyPassed = 1,
  kNotification_Original = 2,
  kQuickSettings = 3,
  kSettingsMainPage = 4,
  kSettingsAboutPage = 5,
  kMaxValue = kSettingsAboutPage
};

// The UMA histogram name for the metric which records incentive show source.
static constexpr char kEolIncentiveShowSourceHistogramName[] =
    "Ash.EndOfLife.IncentiveShowSource";

// Record the UMA metric for where an end of life incentive was shown.
void RecordShowSourceHistogram(EolIncentiveShowSource source);

// This enum is used to record UMA histogram values and should not be
// reordered. Please keep in sync with 'EolIncentiveButtonType' in
// src/tools/metrics/histograms/enums.xml.
enum class EolIncentiveButtonType {
  kNotification_Offer_Approaching = 0,
  kNotification_NoOffer_Approaching = 1,
  kNotification_AboutUpdates_Approaching = 2,
  kNotification_Offer_RecentlyPassed = 3,
  kNotification_NoOffer_RecentlyPassed = 4,
  kNotification_AboutUpdates_RecentlyPassed = 5,
  kNotification_Original_LearnMore = 6,
  kNotification_Original_Dismiss = 7,
  kQuickSettings_Offer_RecentlyPassed = 8,
  kQuickSettings_NoOffer_RecentlyPassed = 9,
  kQuickSettings_NoOffer_Passed = 10,
  kMaxValue = kQuickSettings_NoOffer_Passed
};

// The UMA histogram name for the metric which records incentive button clicks.
static constexpr char kEolIncentiveURLButtonClicked[] =
    "Ash.EndOfLife.IncentiveButtonClicked";

void RecordButtonClicked(EolIncentiveButtonType type);

}  // namespace ash::eol_incentive_util

#endif  // CHROME_BROWSER_ASH_EOL_EOL_INCENTIVE_UTIL_H_
