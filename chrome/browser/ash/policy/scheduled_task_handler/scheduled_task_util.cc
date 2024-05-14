// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "base/values.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"

namespace policy {
namespace {

constexpr base::TimeDelta kDefaultGracePeriod = base::Hours(1);

ScheduledTaskExecutor::Frequency GetFrequency(const std::string& frequency) {
  if (frequency == "DAILY")
    return ScheduledTaskExecutor::Frequency::kDaily;

  if (frequency == "WEEKLY")
    return ScheduledTaskExecutor::Frequency::kWeekly;

  DCHECK_EQ(frequency, "MONTHLY");
  return ScheduledTaskExecutor::Frequency::kMonthly;
}

// Convert the string day of week to UCalendarDaysOfWeek.
UCalendarDaysOfWeek StringDayOfWeekToIcuDayOfWeek(
    const std::string& day_of_week) {
  if (day_of_week == "SUNDAY")
    return UCAL_SUNDAY;
  if (day_of_week == "MONDAY")
    return UCAL_MONDAY;
  if (day_of_week == "TUESDAY")
    return UCAL_TUESDAY;
  if (day_of_week == "WEDNESDAY")
    return UCAL_WEDNESDAY;
  if (day_of_week == "THURSDAY")
    return UCAL_THURSDAY;
  if (day_of_week == "FRIDAY")
    return UCAL_FRIDAY;
  DCHECK_EQ(day_of_week, "SATURDAY");
  return UCAL_SATURDAY;
}

bool IsAfter(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  if (a.after(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  return false;
}

// Returns a valid time based on the policy represented by
// |scheduled_task_data|.
std::unique_ptr<icu::Calendar> SnapToValidTimeBasedOnPolicy(
    const icu::Calendar& time,
    const ScheduledTaskExecutor::ScheduledTaskData& scheduled_task_data) {
  auto res_time = base::WrapUnique(time.clone());

  // Set the daily fields first as they will be common across different policy
  // types.
  res_time->set(UCAL_HOUR_OF_DAY, scheduled_task_data.hour);
  res_time->set(UCAL_MINUTE, scheduled_task_data.minute);
  res_time->set(UCAL_SECOND, 0);
  res_time->set(UCAL_MILLISECOND, 0);

  switch (scheduled_task_data.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      return res_time;

    case ScheduledTaskExecutor::Frequency::kWeekly:
      DCHECK(scheduled_task_data.day_of_week);
      res_time->set(UCAL_DAY_OF_WEEK, scheduled_task_data.day_of_week.value());
      return res_time;

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      DCHECK(scheduled_task_data.day_of_month);
      UErrorCode status = U_ZERO_ERROR;
      // If policy's |day_of_month| is greater than the maximum days in |time|'s
      // current month then it's set to the last day in the month.
      int cur_max_days_in_month =
          res_time->getActualMaximum(UCAL_DAY_OF_MONTH, status);
      DCHECK(U_SUCCESS(status));

      res_time->set(UCAL_DAY_OF_MONTH,
                    std::min(scheduled_task_data.day_of_month.value(),
                             cur_max_days_in_month));
      return res_time;
    }
  }
}

UCalendarDateFields GetFieldToAdvanceFor(
    ScheduledTaskExecutor::Frequency frequency) {
  switch (frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      return UCAL_DAY_OF_MONTH;
    case ScheduledTaskExecutor::Frequency::kWeekly:
      return UCAL_WEEK_OF_YEAR;
    case ScheduledTaskExecutor::Frequency::kMonthly:
      return UCAL_MONTH;
  }
  NOTREACHED_IN_MIGRATION();
}

// Returns a valid time that is advanced in comparison to |time| based on the
// policy represented by |scheduled_task_data|.
//
// For daily policy - Advances |time| by 1 day.
// For weekly policy - Advances |time| by 1 week.
// For monthly policy - Advances |time| by 1 month.
std::unique_ptr<icu::Calendar> AdvanceToNextValidTimeBasedOnPolicy(
    const icu::Calendar& time,
    const ScheduledTaskExecutor::ScheduledTaskData& scheduled_task_data) {
  auto res_time = base::WrapUnique(time.clone());
  UErrorCode status = U_ZERO_ERROR;
  res_time->add(GetFieldToAdvanceFor(scheduled_task_data.frequency), 1, status);
  DCHECK(U_SUCCESS(status));
  // Need to run SnapToValid again, as we might be in a month with less days
  // than the previous month.
  return SnapToValidTimeBasedOnPolicy(*res_time, scheduled_task_data);
}

}  // namespace

namespace scheduled_task_util {
std::optional<ScheduledTaskExecutor::ScheduledTaskData> ParseScheduledTask(
    const base::Value& value,
    const std::string& task_time_field_name) {
  const base::Value::Dict& dict = value.GetDict();
  ScheduledTaskExecutor::ScheduledTaskData result;
  // Parse mandatory values first i.e. hour, minute and frequency of update
  // check. These should always be present due to schema validation at higher
  // layers.
  const base::Value::Dict* task_time_field_dict =
      dict.FindDict(task_time_field_name);
  DCHECK(task_time_field_dict);
  std::optional<int> hour_opt = task_time_field_dict->FindInt("hour");
  DCHECK(hour_opt);
  // Validated by schema validation at higher layers.
  DCHECK(*hour_opt >= 0 && *hour_opt <= 23);
  result.hour = *hour_opt;

  std::optional<int> minute_opt = task_time_field_dict->FindInt("minute");
  DCHECK(minute_opt);
  // Validated by schema validation at higher layers.
  DCHECK(*minute_opt >= 0 && *minute_opt <= 59);
  result.minute = *minute_opt;

  // Validated by schema validation at higher layers.
  const std::string* frequency = dict.FindString({"frequency"});
  DCHECK(frequency);
  result.frequency = GetFrequency(*frequency);

  // Parse extra fields for weekly and monthly frequencies.
  switch (result.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      break;

    case ScheduledTaskExecutor::Frequency::kWeekly: {
      const std::string* day_of_week = dict.FindString({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return std::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToIcuDayOfWeek(*day_of_week);
      break;
    }

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      std::optional<int> day_of_month = dict.FindInt("day_of_month");
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return std::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_month = day_of_month.value();
      break;
    }
  }

  return result;
}

base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  UDate a_ms = a.getTime(status);
  DCHECK(U_SUCCESS(status));
  UDate b_ms = b.getTime(status);
  DCHECK(U_SUCCESS(status));
  DCHECK(a_ms >= b_ms);
  return base::Milliseconds(a_ms - b_ms);
}

std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz) {
  // Get ms from epoch for |cur_time| and use it to get the new time in |tz|.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal_tz =
      std::make_unique<icu::GregorianCalendar>(tz, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }
  // Erase current time from the calendar.
  cal_tz->clear();
  // Use Time::InMillisecondsSinceUnixEpoch() to get ms since epoch in int64_t
  // format.
  cal_tz->setTime(cur_time.InMillisecondsSinceUnixEpoch(), status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }

  return cal_tz;
}

std::optional<base::TimeDelta> CalculateNextScheduledTaskTimerDelay(
    const ScheduledTaskExecutor::ScheduledTaskData& data,
    base::Time time,
    const icu::TimeZone& time_zone) {
  const auto cal = ConvertUtcToTzIcuTime(time, time_zone);
  if (!cal) {
    LOG(ERROR) << "Failed to get current ICU time";
    return std::nullopt;
  }

  auto scheduled_task_time = CalculateNextScheduledTimeAfter(data, *cal);

  return GetDiff(*scheduled_task_time, *cal);
}

std::unique_ptr<icu::Calendar> CalculateNextScheduledTimeAfter(
    const ScheduledTaskExecutor::ScheduledTaskData& data,
    const icu::Calendar& time) {
  auto scheduled_task_time = SnapToValidTimeBasedOnPolicy(time, data);

  // If the time has already passed it means that the scheduled task needs to be
  // advanced based on the policy i.e. by a day, week or month. The equal to
  // case happens when the timer_expired_cb runs and sets the next
  // |scheduled_task_timer_|. In this case |scheduled_task_time| definitely
  // needs to advance as per the policy.
  if (!IsAfter(*scheduled_task_time, time)) {
    scheduled_task_time =
        AdvanceToNextValidTimeBasedOnPolicy(*scheduled_task_time, data);
  }

  DCHECK(IsAfter(*scheduled_task_time, time));

  return scheduled_task_time;
}

// Returns grace from commandline if present and valid. Returns default grace
// time otherwise.
base::TimeDelta GetScheduledRebootGracePeriod() {
  const base::CommandLine* command_line =
      base::CommandLine::ForCurrentProcess();

  std::string grace_time_string = command_line->GetSwitchValueASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting);

  if (grace_time_string.empty()) {
    return kDefaultGracePeriod;
  }

  int grace_time_in_seconds;
  if (!base::StringToInt(grace_time_string, &grace_time_in_seconds) ||
      grace_time_in_seconds < 0) {
    LOG(ERROR) << "Ignored "
               << ash::switches::kScheduledRebootGracePeriodInSecondsForTesting
               << "=" << grace_time_string;
    return kDefaultGracePeriod;
  }

  return base::Seconds(grace_time_in_seconds);
}

bool ShouldSkipRebootDueToGracePeriod(base::Time boot_time,
                                      base::Time reboot_time) {
  // Skip reboot if reboot scheduled within [boot time, boot time + grace time]
  // interval.
  return boot_time + GetScheduledRebootGracePeriod() >= reboot_time;
}

}  // namespace scheduled_task_util
}  // namespace policy
