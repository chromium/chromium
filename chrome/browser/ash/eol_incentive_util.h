// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_EOL_INCENTIVE_UTIL_H_
#define CHROME_BROWSER_ASH_EOL_INCENTIVE_UTIL_H_

class Profile;

namespace base {
class Time;
}  // namespace base

namespace ash::eol_incentive_util {

// The type of EOL incentive to be shown.
enum EolIncentiveType {
  // No EOL incentive is shown.
  kNone,

  // The case where the EOL incentive is shown because of an approaching EOL
  // date.
  kEolApproaching,

  // The case where the EOL incentive is shown because of a recently passed EOL
  // date.
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

}  // namespace ash::eol_incentive_util

#endif  // CHROME_BROWSER_ASH_EOL_INCENTIVE_UTIL_H_
