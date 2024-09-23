// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "base/check.h"
#include "base/check_op.h"
#include "base/json/json_reader.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace policy {

namespace {
void DecodeJsonStringAndNormalize(const std::string& json_string,
                                  base::Value* value) {
  auto parsed_json = base::JSONReader::ReadAndReturnValueWithError(
      json_string, base::JSON_ALLOW_TRAILING_COMMAS);
  ASSERT_TRUE(parsed_json.has_value()) << parsed_json.error().message;
  *value = std::move(*parsed_json);
}

// Creates a JSON policy for daily device scheduled tasks.
std::string CreateDailyScheduledTaskPolicyJson(
    const std::string& task_time_field_name,
    int hour,
    int minute) {
  return base::StringPrintf(
      "{\"%s\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"DAILY\"}",
      task_time_field_name.c_str(), hour, minute);
}

// Creates a JSON policy for weekly device scheduled tasks.
std::string CreateWeeklyScheduledTaskPolicyJson(
    const std::string& task_time_field_name,
    int hour,
    int minute,
    const std::string& day_of_week) {
  return base::StringPrintf(
      "{\"%s\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"WEEKLY\", \"day_of_week\": \"%s\"}",
      task_time_field_name.c_str(), hour, minute, day_of_week.c_str());
}

// Creates a JSON policy for monthly device scheduled tasks.
std::string CreateMonthlyScheduledTaskPolicyJson(
    const std::string& task_time_field_name,
    int hour,
    int minute,
    int day_of_month) {
  return base::StringPrintf(
      "{\"%s\": {\"hour\": %d, \"minute\":  %d}, \"frequency\": "
      "\"MONTHLY\", \"day_of_month\": %d}",
      task_time_field_name.c_str(), hour, minute, day_of_month);
}

// Converts day of week from UCalendarDaysOfWeek to string.
std::string IcuDayOfWeekToStringDayOfWeek(UCalendarDaysOfWeek day_of_week) {
  switch (day_of_week) {
    case UCAL_SUNDAY:
      return "SUNDAY";
    case UCAL_MONDAY:
      return "MONDAY";
    case UCAL_TUESDAY:
      return "TUESDAY";
    case UCAL_WEDNESDAY:
      return "WEDNESDAY";
    case UCAL_THURSDAY:
      return "THURSDAY";
    case UCAL_FRIDAY:
      return "FRIDAY";
    case UCAL_SATURDAY:
      break;
  }
  DCHECK_EQ(day_of_week, UCAL_SATURDAY);
  return "SATURDAY";
}

// Sets |output|'s time of day to |input|'s. Assume's |input| is valid.
void SetTimeOfDay(const icu::Calendar& input, icu::Calendar* output) {
  // Getting each of these properties should succeed if |input| is valid.
  UErrorCode status = U_ZERO_ERROR;
  int32_t hour = input.get(UCAL_HOUR_OF_DAY, status);
  ASSERT_TRUE(U_SUCCESS(status));
  int32_t minute = input.get(UCAL_MINUTE, status);
  ASSERT_TRUE(U_SUCCESS(status));
  int32_t seconds = input.get(UCAL_SECOND, status);
  ASSERT_TRUE(U_SUCCESS(status));
  int32_t ms = input.get(UCAL_MILLISECOND, status);
  ASSERT_TRUE(U_SUCCESS(status));

  output->set(UCAL_HOUR_OF_DAY, hour);
  output->set(UCAL_MINUTE, minute);
  output->set(UCAL_SECOND, seconds);
  output->set(UCAL_MILLISECOND, ms);
}
}  // namespace

namespace scheduled_task_test_util {
base::TimeDelta CalculateTimerExpirationDelayInDailyPolicyForTimeZone(
    base::Time cur_time,
    base::TimeDelta delay,
    const icu::TimeZone& old_tz,
    const icu::TimeZone& new_tz) {
  DCHECK(!delay.is_zero());

  auto cur_time_utc_cal = scheduled_task_util::ConvertUtcToTzIcuTime(
      cur_time, *icu::TimeZone::getGMT());

  auto old_tz_timer_expiration_cal =
      scheduled_task_util::ConvertUtcToTzIcuTime(cur_time + delay, old_tz);

  auto new_tz_timer_expiration_cal =
      scheduled_task_util::ConvertUtcToTzIcuTime(cur_time, new_tz);
  SetTimeOfDay(*old_tz_timer_expiration_cal, new_tz_timer_expiration_cal.get());

  base::TimeDelta result = scheduled_task_util::GetDiff(
      *new_tz_timer_expiration_cal, *cur_time_utc_cal);
  // If the scheduled task time in the new time zone has already passed then it
  // will happen on the next day.
  if (result <= base::TimeDelta())
    result += base::Days(1);
  return result;
}

int GetDaysInMonthInEpochYear(UCalendarMonths month) {
  switch (month) {
    case UCAL_JANUARY:
    case UCAL_MARCH:
    case UCAL_MAY:
    case UCAL_JULY:
    case UCAL_AUGUST:
    case UCAL_OCTOBER:
    case UCAL_DECEMBER:
      return 31;
    case UCAL_FEBRUARY:
      return 28;
    case UCAL_APRIL:
    case UCAL_JUNE:
    case UCAL_SEPTEMBER:
    case UCAL_NOVEMBER:
      return 30;
    case UCAL_UNDECIMBER:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return -1;
}

bool AdvanceTimeAndSetDayOfMonth(int day_of_month, icu::Calendar* time) {
  DCHECK(time);
  UErrorCode status = U_ZERO_ERROR;
  time->add(UCAL_DAY_OF_MONTH, 1, status);
  if (U_FAILURE(status)) {
    ADD_FAILURE() << "Failed to advance month";
    return false;
  }

  // Cap day of month to a valid day in the incremented month.
  int cur_max_days_in_month = time->getActualMaximum(UCAL_DAY_OF_MONTH, status);
  if (U_FAILURE(status)) {
    ADD_FAILURE() << "Failed to get max days in month";
    return false;
  }
  time->set(UCAL_DAY_OF_MONTH, std::min(day_of_month, cur_max_days_in_month));
  return true;
}

std::pair<base::Value, std::unique_ptr<icu::Calendar>> CreatePolicy(
    const icu::TimeZone& time_zone,
    base::Time current_time,
    base::TimeDelta delay,
    ScheduledTaskExecutor::Frequency frequency,
    const std::string& task_time_field_name) {
  // Calculate time from one hour from now and set the policy to
  // happen daily at that time.
  base::Time scheduled_task_time = current_time + delay;
  auto scheduled_task_icu_time = scheduled_task_util::ConvertUtcToTzIcuTime(
      scheduled_task_time, time_zone);

  // Extracting fields from valid ICU time should always succeed.
  UErrorCode status = U_ZERO_ERROR;
  int32_t hour = scheduled_task_icu_time->get(UCAL_HOUR_OF_DAY, status);
  DCHECK(U_SUCCESS(status));
  int32_t minute = scheduled_task_icu_time->get(UCAL_MINUTE, status);
  DCHECK(U_SUCCESS(status));
  int32_t day_of_week = scheduled_task_icu_time->get(UCAL_DAY_OF_WEEK, status);
  DCHECK(U_SUCCESS(status));
  int32_t day_of_month =
      scheduled_task_icu_time->get(UCAL_DAY_OF_MONTH, status);
  DCHECK(U_SUCCESS(status));

  base::Value scheduled_task_value;
  switch (frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily: {
      DecodeJsonStringAndNormalize(CreateDailyScheduledTaskPolicyJson(
                                       task_time_field_name, hour, minute),
                                   &scheduled_task_value);
      break;
    }

    case ScheduledTaskExecutor::Frequency::kWeekly: {
      DecodeJsonStringAndNormalize(
          CreateWeeklyScheduledTaskPolicyJson(
              task_time_field_name, hour, minute,
              IcuDayOfWeekToStringDayOfWeek(
                  static_cast<UCalendarDaysOfWeek>(day_of_week))),
          &scheduled_task_value);
      break;
    }

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      DecodeJsonStringAndNormalize(
          CreateMonthlyScheduledTaskPolicyJson(task_time_field_name, hour,
                                               minute, day_of_month),
          &scheduled_task_value);
      break;
    }
  }
  return std::make_pair(std::move(scheduled_task_value),
                        std::move(scheduled_task_icu_time));
}

base::Time IcuToBaseTime(const icu::Calendar& time) {
  UErrorCode status = U_ZERO_ERROR;
  UDate seconds_from_epoch = time.getTime(status) / 1000;
  DCHECK(U_SUCCESS(status));
  base::Time result = base::Time::FromTimeT(seconds_from_epoch);
  if (result.is_null())
    result = base::Time::UnixEpoch();
  return result;
}

}  // namespace scheduled_task_test_util

}  // namespace policy
