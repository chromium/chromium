// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include "chrome/browser/chromeos/policy/scheduled_task_handler/scheduled_task_util.h"

#include <string>

#include "base/check.h"
#include "base/logging.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/icu/source/i18n/unicode/calendar.h"

namespace policy {
namespace {
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
}  // namespace

namespace scheduled_task_util {
absl::optional<ScheduledTaskExecutor::ScheduledTaskData> ParseScheduledTask(
    const base::Value& value,
    const std::string& task_time_field_name) {
  ScheduledTaskExecutor::ScheduledTaskData result;
  // Parse mandatory values first i.e. hour, minute and frequency of update
  // check. These should always be present due to schema validation at higher
  // layers.
  const base::Value* hour_value = value.FindPathOfType(
      {task_time_field_name, "hour"}, base::Value::Type::INTEGER);
  DCHECK(hour_value);
  int hour = hour_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(hour >= 0 && hour <= 23);
  result.hour = hour;

  const base::Value* minute_value = value.FindPathOfType(
      {task_time_field_name, "minute"}, base::Value::Type::INTEGER);
  DCHECK(minute_value);
  int minute = minute_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(minute >= 0 && minute <= 59);
  result.minute = minute;

  // Validated by schema validation at higher layers.
  const std::string* frequency = value.FindStringKey({"frequency"});
  DCHECK(frequency);
  result.frequency = GetFrequency(*frequency);

  // Parse extra fields for weekly and monthly frequencies.
  switch (result.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      break;

    case ScheduledTaskExecutor::Frequency::kWeekly: {
      const std::string* day_of_week = value.FindStringKey({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return absl::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToIcuDayOfWeek(*day_of_week);
      break;
    }

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      absl::optional<int> day_of_month = value.FindIntKey({"day_of_month"});
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return absl::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_month = day_of_month.value();
      break;
    }
  }

  return result;
}
}  // namespace scheduled_task_util
}  // namespace policy
