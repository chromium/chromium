// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/settings_updated_metrics_info.h"

#include "base/containers/fixed_flat_map.h"
#include "base/json/values_util.h"
#include "base/time/time.h"
#include "base/values.h"

namespace ash {

namespace {

using TimePeriod = SettingsUpdatedMetricsInfo::TimePeriod;
using Category = SettingsUpdatedMetricsInfo::Category;

constexpr char kLocalFirstConnectionKey[] = "initial_connection_time";
constexpr char kCategoryKey[] = "category";

constexpr auto kTimePeriodToKey =
    base::MakeFixedFlatMap<TimePeriod, const char*>({
        {TimePeriod::kOneHour, "one_hour"},
        {TimePeriod::kThreeHours, "three_hours"},
        {TimePeriod::kOneDay, "one_day"},
        {TimePeriod::kThreeDays, "three_days"},
        {TimePeriod::kOneWeek, "one_week"},
    });

constexpr auto kTimePeriodToTimeDelta =
    base::MakeFixedFlatMap<TimePeriod, base::TimeDelta>({
        {TimePeriod::kOneHour, base::Hours(1)},
        {TimePeriod::kThreeHours, base::Hours(3)},
        {TimePeriod::kOneDay, base::Days(1)},
        {TimePeriod::kThreeDays, base::Days(3)},
        {TimePeriod::kOneWeek, base::Days(7)},
    });

bool IsValidCategory(int category_int) {
  return category_int >= static_cast<int>(Category::kMinValue) &&
         category_int <= static_cast<int>(Category::kMaxValue);
}

}  // namespace

SettingsUpdatedMetricsInfo::SettingsUpdatedMetricsInfo(
    Category category,
    base::Time initial_connection_time)
    : category_(category),
      initial_connection_time_(initial_connection_time),
      time_period_counts_({0}) {}
SettingsUpdatedMetricsInfo::~SettingsUpdatedMetricsInfo() = default;
SettingsUpdatedMetricsInfo::SettingsUpdatedMetricsInfo(
    const SettingsUpdatedMetricsInfo&) = default;

// static
std::optional<SettingsUpdatedMetricsInfo> SettingsUpdatedMetricsInfo::FromDict(
    const base::Value::Dict& dict) {
  auto category_int = dict.FindInt(kCategoryKey);
  if (!category_int || !IsValidCategory(*category_int)) {
    return std::nullopt;
  }

  auto initial_connection_time_ =
      base::ValueToTime(dict.Find(kLocalFirstConnectionKey));
  if (!initial_connection_time_) {
    return std::nullopt;
  }

  SettingsUpdatedMetricsInfo metric_info(static_cast<Category>(*category_int),
                                         *initial_connection_time_);
  for (const auto& [time_period, key] : kTimePeriodToKey) {
    metric_info.time_period_counts_[static_cast<int>(time_period)] =
        dict.FindInt(key).value_or(0);
  }

  return metric_info;
}

std::optional<TimePeriod> SettingsUpdatedMetricsInfo::RecordSettingsUpdate(
    base::Time update_time) {
  CHECK(update_time >= initial_connection_time_);
  const base::TimeDelta time_since_initial_connection =
      update_time - initial_connection_time_;

  for (const auto& [time_period, time_delta] : kTimePeriodToTimeDelta) {
    if (time_since_initial_connection < time_delta) {
      time_period_counts_[static_cast<size_t>(time_period)] += 1;
      return time_period;
    }
  }

  return std::nullopt;
}

base::Value::Dict SettingsUpdatedMetricsInfo::ToDict() const {
  base::Value::Dict dict;
  dict.Set(kLocalFirstConnectionKey,
           base::TimeToValue(initial_connection_time_));
  dict.Set(kCategoryKey, static_cast<int>(category_));

  for (size_t i = 0; i < time_period_counts_.size(); ++i) {
    if (time_period_counts_[i] > 0) {
      TimePeriod time_period = static_cast<TimePeriod>(i);
      DCHECK(kTimePeriodToKey.contains(time_period));
      dict.Set(kTimePeriodToKey.at(time_period), time_period_counts_[i]);
    }
  }

  return dict;
}

int SettingsUpdatedMetricsInfo::GetCount(TimePeriod time_period) const {
  return time_period_counts_[static_cast<int>(time_period)];
}

}  // namespace ash
