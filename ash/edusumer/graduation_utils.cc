// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/edusumer/graduation_utils.h"

#include <optional>

#include "ash/constants/ash_pref_names.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "base/values.h"
#include "components/prefs/pref_service.h"

namespace ash::graduation {

namespace {
// Dictionary keys used for accessing kGraduationEnablementStatus pref values.
constexpr char kIsEnabledKey[] = "is_enabled";
constexpr char kStartDateKey[] = "start_date";
constexpr char kEndDateKey[] = "end_date";
constexpr char kDayKey[] = "day";
constexpr char kMonthKey[] = "month";
constexpr char kYearKey[] = "year";

// Returns a Time object representing a date at local midnight if the date is
// valid. Returns std::nullopt if the date is invalid.
std::optional<base::Time> GetLocalMidnightTimeForDate(
    const base::Value::Dict* date) {
  CHECK(date);
  std::optional<int> day = date->FindInt(kDayKey);
  std::optional<int> month = date->FindInt(kMonthKey);
  std::optional<int> year = date->FindInt(kYearKey);
  if (!day || !month || !year) {
    return std::nullopt;
  }

  base::Time::Exploded exploded;
  exploded.day_of_month = *day;
  exploded.month = *month;
  exploded.year = *year;
  exploded.hour = 0;
  exploded.minute = 0;
  exploded.second = 0;
  exploded.millisecond = 0;

  base::Time time;
  bool date_exists = base::Time::FromLocalExploded(exploded, &time);
  if (!date_exists) {
    return std::nullopt;
  }
  return time.LocalMidnight();
}
}  // namespace

bool IsEligibleForGraduation(PrefService* pref_service) {
  CHECK(pref_service);
  const base::Value::Dict& graduation_policy_pref =
      pref_service->GetDict(prefs::kGraduationEnablementStatus);
  if (graduation_policy_pref.empty()) {
    return false;
  }

  bool is_enabled =
      graduation_policy_pref.FindBool(kIsEnabledKey).value_or(false);
  if (!is_enabled) {
    return false;
  }

  // Compare the current date at local midnight time to the start and end dates
  // at local midnight time.
  base::Time current_time = base::Time::Now().LocalMidnight();

  const base::Value::Dict* start_date_dict =
      graduation_policy_pref.FindDict(kStartDateKey);
  if (start_date_dict) {
    std::optional<base::Time> start_time =
        GetLocalMidnightTimeForDate(start_date_dict);
    if (!start_time) {
      return false;
    }
    if (current_time < *start_time) {
      return false;
    }
  }

  const base::Value::Dict* end_date_dict =
      graduation_policy_pref.FindDict(kEndDateKey);
  if (end_date_dict) {
    std::optional<base::Time> end_time =
        GetLocalMidnightTimeForDate(end_date_dict);
    if (!end_time) {
      return false;
    }
    if (current_time > *end_time) {
      return false;
    }
  }

  return true;
}
}  // namespace ash::graduation
