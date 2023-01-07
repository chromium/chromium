// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_SCHEDULED_TASK_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_SCHEDULED_TASK_TEST_UTIL_H_

#include <memory>
#include <string>
#include <utility>

#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace policy {

namespace scheduled_task_test_util {

// Calculates |cur_time + delay| in |old_tz|. Then gets the same time of day
// (hours:minutes:seconds:ms) in |new_tz|. Returns the delay between |cur_time|
// and |new_tz|. |delay| must be non-zero.
base::TimeDelta CalculateTimerExpirationDelayInDailyPolicyForTimeZone(
    base::Time cur_time,
    base::TimeDelta delay,
    const icu::TimeZone& old_tz,
    const icu::TimeZone& new_tz);

// Returns the number of days in |month| in the epoch year i.e. 1970.
int GetDaysInMonthInEpochYear(UCalendarMonths month);

// Advances the month in time and sets day to min(|day_of_month|, max days in
// new month). Returns true if |time| is valid after these operations, false
// otherwise.
bool AdvanceTimeAndSetDayOfMonth(int day_of_month, icu::Calendar* time);

// Creates a scheduled task policy starting at a delay of |delay| from
// |current_time| and recurring with frequency |frequency|. Returns the policy
// and the first scheduled task time.
std::pair<base::Value, std::unique_ptr<icu::Calendar>> CreatePolicy(
    const icu::TimeZone& time_zone,
    base::Time current_time,
    base::TimeDelta delay,
    ScheduledTaskExecutor::Frequency frequency,
    const std::string& task_time_field_name);

// Converts an icu::Calendar to base::Time. Assumes |time| is valid.
base::Time IcuToBaseTime(const icu::Calendar& time);

}  // namespace scheduled_task_test_util

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_TEST_SCHEDULED_TASK_TEST_UTIL_H_
