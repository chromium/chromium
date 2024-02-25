// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_
#define CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_

#include <optional>
#include <string>

#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"

namespace policy {

// Utility methods for scheduled device policies.
namespace scheduled_task_util {

// Parses |value| into a |ScheduledTaskData|. Returns nullopt if there
// is any error while parsing |value|.
//
// It is expected that the value has the following fields:
// |task_time_field_name| - time of the day when the task should occur. The name
// of the field is passed as an argument to ParseScheduledTask method.
// |frequency| - frequency of reccurring task. Can be daily, weekly or monthly.
// |day_of_week| - optional field, used for policies that recur weekly.
// |day_of_month| - optional field, used for policies that recur monthly.
std::optional<ScheduledTaskExecutor::ScheduledTaskData> ParseScheduledTask(
    const base::Value& value,
    const std::string& task_time_field_name);

// Calculates the difference in milliseconds of |a| - |b|. Caller has to ensure
// |a| >= |b|.
base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b);

// Converts |cur_time| to ICU time in the time zone |tz|.
std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz);

// Calculates the delay from |time| at which the policy defined through |data|
// should run next. Returns nullopt if the calculation failed due to a
// concurrent DST or Time Zone change.
// |time_zone| refers to the time zone that should be considered for the policy.
std::optional<base::TimeDelta> CalculateNextScheduledTaskTimerDelay(
    const ScheduledTaskExecutor::ScheduledTaskData& data,
    const base::Time time,
    const icu::TimeZone& time_zone);

// Calculates the next scheduled calendar event that lies after |time|
// in accordance with the policy.
std::unique_ptr<icu::Calendar> CalculateNextScheduledTimeAfter(
    const ScheduledTaskExecutor::ScheduledTaskData& data,
    const icu::Calendar& time);

// Returns grace period from commandline if present and valid. Returns default
// grace time otherwise.
base::TimeDelta GetScheduledRebootGracePeriod();

// Returns true if `reboot_time` is within grace time period.
bool ShouldSkipRebootDueToGracePeriod(base::Time boot_time,
                                      base::Time reboot_time);

}  // namespace scheduled_task_util

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_SCHEDULED_TASK_HANDLER_SCHEDULED_TASK_UTIL_H_
