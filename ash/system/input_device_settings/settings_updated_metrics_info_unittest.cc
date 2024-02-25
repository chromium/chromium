// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/input_device_settings/settings_updated_metrics_info.h"

#include "base/json/values_util.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using TimePeriod = SettingsUpdatedMetricsInfo::TimePeriod;
using Category = SettingsUpdatedMetricsInfo::Category;

constexpr char kLocalFirstConnectionKey[] = "initial_connection_time";
constexpr char kCategoryKey[] = "category";

}  // namespace

TEST(SettingsUpdatedMetricsInfoTest, ConvertToDict) {
  {
    const base::Time expected_time =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));

    SettingsUpdatedMetricsInfo metrics_info(
        SettingsUpdatedMetricsInfo::Category::kFirstEver, expected_time);

    auto metrics_info_dict = metrics_info.ToDict();
    auto category_int = metrics_info_dict.FindInt(kCategoryKey);
    ASSERT_TRUE(category_int);

    auto* time_value = metrics_info_dict.Find(kLocalFirstConnectionKey);
    ASSERT_TRUE(time_value);
    auto time_from_dict = base::ValueToTime(*time_value);
    ASSERT_TRUE(time_from_dict);

    EXPECT_EQ(expected_time, *time_from_dict);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kFirstEver,
              static_cast<SettingsUpdatedMetricsInfo::Category>(*category_int));
  }

  {
    const base::Time expected_time =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(20000));

    SettingsUpdatedMetricsInfo metrics_info(
        SettingsUpdatedMetricsInfo::Category::kDefault, expected_time);

    auto metrics_info_dict = metrics_info.ToDict();
    auto category_int = metrics_info_dict.FindInt(kCategoryKey);
    ASSERT_TRUE(category_int);

    auto* time_value = metrics_info_dict.Find(kLocalFirstConnectionKey);
    ASSERT_TRUE(time_value);
    auto time_from_dict = base::ValueToTime(*time_value);
    ASSERT_TRUE(time_from_dict);

    EXPECT_EQ(expected_time, *time_from_dict);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kDefault,
              static_cast<SettingsUpdatedMetricsInfo::Category>(*category_int));
  }

  {
    const base::Time expected_time =
        base::Time::FromDeltaSinceWindowsEpoch(base::Days(30000));

    SettingsUpdatedMetricsInfo metrics_info(
        SettingsUpdatedMetricsInfo::Category::kSynced, expected_time);

    auto metrics_info_dict = metrics_info.ToDict();
    auto category_int = metrics_info_dict.FindInt(kCategoryKey);
    ASSERT_TRUE(category_int);

    auto* time_value = metrics_info_dict.Find(kLocalFirstConnectionKey);
    ASSERT_TRUE(time_value);
    auto time_from_dict = base::ValueToTime(*time_value);
    ASSERT_TRUE(time_from_dict);

    EXPECT_EQ(expected_time, *time_from_dict);
    EXPECT_EQ(SettingsUpdatedMetricsInfo::Category::kSynced,
              static_cast<SettingsUpdatedMetricsInfo::Category>(*category_int));
  }
}

TEST(SettingsUpdatedMetricsInfoTest, InvalidConversionMissingTime) {
  base::Value::Dict dict;
  dict.Set(kCategoryKey, static_cast<int>(Category::kFirstEver));
  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  EXPECT_FALSE(metrics_info);
}

TEST(SettingsUpdatedMetricsInfoTest, InvalidConversionMissingCategory) {
  base::Value::Dict dict;
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(base::Time::Now()));
  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  EXPECT_FALSE(metrics_info);
}

TEST(SettingsUpdatedMetricsInfoTest, ConversionNoPeriods) {
  base::Value::Dict dict;
  const base::Time expected_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  dict.Set(kCategoryKey, static_cast<int>(Category::kFirstEver));
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(expected_time));
  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  ASSERT_TRUE(metrics_info);
  EXPECT_EQ(Category::kFirstEver, metrics_info->category());
  EXPECT_EQ(expected_time, metrics_info->initial_connection_time());
}

TEST(SettingsUpdatedMetricsInfoTest, ConversionInvalidCategory) {
  base::Value::Dict dict;
  const base::Time expected_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  dict.Set(kCategoryKey, static_cast<int>(Category::kMaxValue) + 1);
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(expected_time));
  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  EXPECT_FALSE(metrics_info);
}

TEST(SettingsUpdatedMetricsInfoTest, ConversionCheckPeriodCounts) {
  base::Value::Dict dict;
  const base::Time expected_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  dict.Set(kCategoryKey, static_cast<int>(Category::kMaxValue));
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(expected_time));

  dict.Set("one_hour", 1);
  dict.Set("three_hours", 2);
  dict.Set("one_day", 3);
  dict.Set("three_days", 4);
  dict.Set("one_week", 5);

  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  ASSERT_TRUE(metrics_info);
  EXPECT_EQ(1, metrics_info->GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(2, metrics_info->GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info->GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(4, metrics_info->GetCount(TimePeriod::kThreeDays));
  EXPECT_EQ(5, metrics_info->GetCount(TimePeriod::kOneWeek));
}

TEST(SettingsUpdatedMetricsInfoTest, ConversionCheckPeriodCountsRoundTrip) {
  base::Value::Dict dict;
  const base::Time expected_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  dict.Set(kCategoryKey, static_cast<int>(Category::kMaxValue));
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(expected_time));
  dict.Set("one_hour", 1);
  dict.Set("three_hours", 2);
  dict.Set("one_day", 3);
  dict.Set("three_days", 4);
  dict.Set("one_week", 5);

  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  ASSERT_TRUE(metrics_info);
  base::Value::Dict duplicate_dict = metrics_info->ToDict();
  EXPECT_EQ(dict, duplicate_dict);
}

TEST(SettingsUpdatedMetricsInfoTest, ConversionCheckPeriodCountsAllZero) {
  base::Value::Dict dict;
  const base::Time expected_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  dict.Set(kCategoryKey, static_cast<int>(Category::kMaxValue));
  dict.Set(kLocalFirstConnectionKey, base::TimeToValue(expected_time));

  auto metrics_info = SettingsUpdatedMetricsInfo::FromDict(dict);
  ASSERT_TRUE(metrics_info);
  EXPECT_EQ(0, metrics_info->GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(0, metrics_info->GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(0, metrics_info->GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(0, metrics_info->GetCount(TimePeriod::kThreeDays));
  EXPECT_EQ(0, metrics_info->GetCount(TimePeriod::kOneWeek));
}

TEST(SettingsUpdatedMetricsInfoTest, CheckCountUpdates) {
  const base::Time start_time =
      base::Time::FromDeltaSinceWindowsEpoch(base::Days(10000));
  std::optional<TimePeriod> optional_period;
  SettingsUpdatedMetricsInfo metrics_info(
      SettingsUpdatedMetricsInfo::Category::kFirstEver, start_time);

  // Update within one hour.
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Minutes(10));
  EXPECT_EQ(TimePeriod::kOneHour, *optional_period);
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Minutes(30));
  EXPECT_EQ(TimePeriod::kOneHour, *optional_period);
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Minutes(50));
  EXPECT_EQ(TimePeriod::kOneHour, *optional_period);
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Minutes(59));
  EXPECT_EQ(TimePeriod::kOneHour, *optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(0, metrics_info.GetCount(TimePeriod::kThreeHours));

  // Update within one day.
  optional_period = metrics_info.RecordSettingsUpdate(
      start_time + base::Hours(3) + base::Minutes(10));
  EXPECT_EQ(TimePeriod::kOneDay, *optional_period);
  optional_period = metrics_info.RecordSettingsUpdate(
      start_time + base::Hours(4) + base::Minutes(30));
  EXPECT_EQ(TimePeriod::kOneDay, *optional_period);
  optional_period = metrics_info.RecordSettingsUpdate(
      start_time + base::Hours(23) + base::Minutes(50));
  EXPECT_EQ(TimePeriod::kOneDay, *optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(0, metrics_info.GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info.GetCount(TimePeriod::kOneDay));

  // Update within 3 days.
  optional_period = metrics_info.RecordSettingsUpdate(
      start_time + base::Days(2) + base::Minutes(10));
  EXPECT_EQ(TimePeriod::kThreeDays, *optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(0, metrics_info.GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info.GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeDays));

  // Update within 1 week.
  optional_period = metrics_info.RecordSettingsUpdate(
      start_time + base::Days(5) + base::Minutes(10));
  EXPECT_EQ(TimePeriod::kOneWeek, *optional_period);
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Days(6));
  EXPECT_EQ(TimePeriod::kOneWeek, *optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(0, metrics_info.GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info.GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeDays));
  EXPECT_EQ(2, metrics_info.GetCount(TimePeriod::kOneWeek));

  // Update within 3 hours, done last to ensure the logic properly skips the
  // various time chunks.
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Hours(2));
  EXPECT_EQ(SettingsUpdatedMetricsInfo::TimePeriod::kThreeHours,
            *optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info.GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeDays));
  EXPECT_EQ(2, metrics_info.GetCount(TimePeriod::kOneWeek));

  // If >1week, nothing changes and return std::nullopt.
  optional_period =
      metrics_info.RecordSettingsUpdate(start_time + base::Days(7));
  EXPECT_FALSE(optional_period);
  EXPECT_EQ(4, metrics_info.GetCount(TimePeriod::kOneHour));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeHours));
  EXPECT_EQ(3, metrics_info.GetCount(TimePeriod::kOneDay));
  EXPECT_EQ(1, metrics_info.GetCount(TimePeriod::kThreeDays));
  EXPECT_EQ(2, metrics_info.GetCount(TimePeriod::kOneWeek));
}

}  // namespace ash
