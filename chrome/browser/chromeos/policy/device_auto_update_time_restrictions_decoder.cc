// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/device_auto_update_time_restrictions_decoder.h"

#include <memory>
#include <string>
#include <vector>

#include "base/optional.h"
#include "base/time/time.h"
#include "chromeos/policy/weekly_time/weekly_time.h"
#include "chromeos/policy/weekly_time/weekly_time_interval.h"

using base::DictionaryValue;
using base::ListValue;
using base::Value;

namespace {

constexpr base::TimeDelta kHour = base::TimeDelta::FromHours(1);
constexpr base::TimeDelta kMinute = base::TimeDelta::FromMinutes(1);

int DayOfWeekStringToInt(const std::string& day_of_week_str) {
  if (day_of_week_str == "Monday")
    return 1;
  if (day_of_week_str == "Tuesday")
    return 2;
  if (day_of_week_str == "Wednesday")
    return 3;
  if (day_of_week_str == "Thursday")
    return 4;
  if (day_of_week_str == "Friday")
    return 5;
  if (day_of_week_str == "Saturday")
    return 6;
  if (day_of_week_str == "Sunday")
    return 7;
  return 0;
}
}  // namespace

namespace policy {

base::Optional<WeeklyTime> WeeklyTimeFromDictValue(
    const DictionaryValue& weekly_time_dict) {
  const Value* day_of_week_val =
      weekly_time_dict.FindKeyOfType("day_of_week", Value::Type::STRING);
  const Value* hours_val =
      weekly_time_dict.FindKeyOfType("hours", Value::Type::INTEGER);
  const Value* minutes_val =
      weekly_time_dict.FindKeyOfType("minutes", Value::Type::INTEGER);
  if (!day_of_week_val || !hours_val || !minutes_val)
    return base::nullopt;

  int day_of_week = DayOfWeekStringToInt(day_of_week_val->GetString());
  if (day_of_week == 0)
    return base::nullopt;
  int hours = hours_val->GetInt();
  int minutes = minutes_val->GetInt();
  int milliseconds =
      hours * kHour.InMilliseconds() + minutes * kMinute.InMilliseconds();
  return WeeklyTime(day_of_week, milliseconds,
                    base::nullopt /* timezone_offset */);
}

bool WeeklyTimeIntervalsFromListValue(
    const ListValue& intervals_list,
    std::vector<WeeklyTimeInterval>* intervals_out) {
  for (const auto& interval : intervals_list.GetList()) {
    const Value* start_val = interval.FindKey("start");
    const Value* end_val = interval.FindKey("end");
    if (!start_val || !end_val)
      return false;
    const DictionaryValue *start_dict, *end_dict;
    if (!start_val->GetAsDictionary(&start_dict) ||
        !end_val->GetAsDictionary(&end_dict))
      return false;
    base::Optional<WeeklyTime> start = WeeklyTimeFromDictValue(*start_dict);
    base::Optional<WeeklyTime> end = WeeklyTimeFromDictValue(*end_dict);
    if (!start || !end)
      return false;

    intervals_out->push_back(WeeklyTimeInterval(start.value(), end.value()));
  }
  return true;
}

}  // namespace policy
