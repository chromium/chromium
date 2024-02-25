// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SYSTEM_INPUT_DEVICE_SETTINGS_SETTINGS_UPDATED_METRICS_INFO_H_
#define ASH_SYSTEM_INPUT_DEVICE_SETTINGS_SETTINGS_UPDATED_METRICS_INFO_H_

#include <array>
#include <cstdint>

#include "ash/ash_export.h"
#include "base/time/time.h"
#include "base/values.h"

namespace ash {

// SettingsUpdatedMetricsInfo is in charge of keeping track of settings updated
// metrics info as well as serializing/deserializing to/from `base::Value`
// objects.
class ASH_EXPORT SettingsUpdatedMetricsInfo {
 public:
  enum class TimePeriod : uint8_t {
    kOneHour,
    kThreeHours,
    kOneDay,
    kThreeDays,
    kOneWeek,
    kMinValue = kOneHour,
    kMaxValue = kOneWeek
  };

  enum class Category {
    kFirstEver = 0,
    kDefault = 1,
    kSynced = 2,
    kMinValue = kFirstEver,
    kMaxValue = kSynced,
  };

  SettingsUpdatedMetricsInfo(Category category,
                             base::Time initial_connection_time);
  ~SettingsUpdatedMetricsInfo();
  SettingsUpdatedMetricsInfo(const SettingsUpdatedMetricsInfo&);

  // Serializes a `SettingsUpdatedMetricsInfo` object from a
  // `base::Value::Dict`.
  // Returns `std::nullopt` if any necessary fields are missing.
  static std::optional<SettingsUpdatedMetricsInfo> FromDict(
      const base::Value::Dict& dict);

  // Converts the object to a `base::Value::Dict` to be stored in prefs.
  base::Value::Dict ToDict() const;

  // Records that an update happened at the given time. Returns the `TimePeriod`
  // that corresponds to the time given. Returns `std::nullopt` if the
  // `update_time` is greater than the longest period we track.
  std::optional<TimePeriod> RecordSettingsUpdate(base::Time update_time);

  // Returns the current count for the given `TimePeriod`.
  int GetCount(TimePeriod) const;

  Category category() const { return category_; }

  base::Time initial_connection_time() const {
    return initial_connection_time_;
  }

 private:
  Category category_;
  base::Time initial_connection_time_;
  std::array<int, static_cast<size_t>(TimePeriod::kMaxValue) + 1>
      time_period_counts_;
};

}  // namespace ash

#endif  // ASH_SYSTEM_INPUT_DEVICE_SETTINGS_SETTINGS_UPDATED_METRICS_INFO_H_
