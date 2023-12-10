// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/time_limits/app_types.h"

#include <string>

#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace app_time {

namespace {
base::Time TimeFromString(const std::string& str) {
  base::Time timestamp;
  EXPECT_TRUE(base::Time::FromString(str.c_str(), &timestamp));
  return timestamp;
}

}  // namespace

using ActiveTimeTest = testing::Test;
using AppActivityTest = testing::Test;

TEST_F(ActiveTimeTest, CreateActiveTime) {
  const base::Time start = TimeFromString("11 Jan 2020 10:00:00 PST");
  const base::Time end = TimeFromString("11 Jan 2020 10:10:00 PST");

  // Create ActiveTime with the correct timestamps.
  const AppActivity::ActiveTime active_time(start, end);
  EXPECT_EQ(start, active_time.active_from());
  EXPECT_EQ(end, active_time.active_to());

  // Try to create ActiveTime with invalid ranges.
  EXPECT_DCHECK_DEATH(AppActivity::ActiveTime(start, start));
  EXPECT_DCHECK_DEATH(AppActivity::ActiveTime(end, start));
}

TEST_F(ActiveTimeTest, UpdateActiveTime) {
  AppActivity::ActiveTime active_time(
      TimeFromString("11 Jan 2020 10:00:00 PST"),
      TimeFromString("11 Jan 2020 10:10:00 PST"));
  const base::Time& start_equal_end = active_time.active_to();
  EXPECT_DCHECK_DEATH(active_time.set_active_from(start_equal_end));

  const base::Time start_after_end = active_time.active_to() + base::Seconds(1);
  EXPECT_DCHECK_DEATH(active_time.set_active_from(start_after_end));

  const base::Time& end_equal_start = active_time.active_from();
  EXPECT_DCHECK_DEATH(active_time.set_active_to(end_equal_start));

  const base::Time end_before_start =
      active_time.active_from() - base::Seconds(1);
  EXPECT_DCHECK_DEATH(active_time.set_active_to(end_before_start));
}

TEST_F(ActiveTimeTest, ActiveTimeTimestampComparisions) {
  const AppActivity::ActiveTime active_time(
      TimeFromString("11 Jan 2020 10:00:00 PST"),
      TimeFromString("11 Jan 2020 10:10:00 PST"));

  const base::Time contained = TimeFromString("11 Jan 2020 10:05:00 PST");
  EXPECT_TRUE(active_time.Contains(contained));
  EXPECT_FALSE(active_time.IsEarlierThan(contained));
  EXPECT_FALSE(active_time.IsLaterThan(contained));

  const base::Time before = TimeFromString("11 Jan 2020 09:58:00 PST");
  EXPECT_FALSE(active_time.Contains(before));
  EXPECT_FALSE(active_time.IsEarlierThan(before));
  EXPECT_TRUE(active_time.IsLaterThan(before));

  const base::Time after = TimeFromString("11 Jan 2020 10:11:00 PST");
  EXPECT_FALSE(active_time.Contains(after));
  EXPECT_TRUE(active_time.IsEarlierThan(after));
  EXPECT_FALSE(active_time.IsLaterThan(after));

  const base::Time& equal_start = active_time.active_from();
  EXPECT_FALSE(active_time.Contains(equal_start));
  EXPECT_FALSE(active_time.IsEarlierThan(equal_start));
  EXPECT_TRUE(active_time.IsLaterThan(equal_start));

  const base::Time& equal_end = active_time.active_to();
  EXPECT_FALSE(active_time.Contains(equal_end));
  EXPECT_TRUE(active_time.IsEarlierThan(equal_end));
  EXPECT_FALSE(active_time.IsLaterThan(equal_end));
}

TEST_F(ActiveTimeTest, MergeActiveTimesTest) {
  const base::TimeDelta delta =
      AppActivity::ActiveTime::kActiveTimeMergePrecision / 2;

  base::Time time1 = TimeFromString("11 Jan 2020 10:00:00 PST");
  base::Time time2 = TimeFromString("11 Jan 2020 10:10:00 PST");
  base::Time time3 = TimeFromString("11 Jan 2020 10:20:00 PST");

  AppActivity::ActiveTime active_time_1(time1, time2);
  AppActivity::ActiveTime active_time_2(time2 + delta, time3);
  AppActivity::ActiveTime active_time_3(time2 + 3 * delta, time3);

  std::optional<AppActivity::ActiveTime> merged_time1 =
      AppActivity::ActiveTime::Merge(active_time_1, active_time_2);
  EXPECT_TRUE(merged_time1.has_value());
  EXPECT_EQ(merged_time1->active_from(), time1);
  EXPECT_EQ(merged_time1->active_to(), time3);

  std::optional<AppActivity::ActiveTime> merged_time2 =
      AppActivity::ActiveTime::Merge(active_time_2, active_time_1);
  EXPECT_TRUE(merged_time2.has_value());
  EXPECT_EQ(merged_time2->active_from(), time1);
  EXPECT_EQ(merged_time2->active_to(), time3);

  std::optional<AppActivity::ActiveTime> merged_time3 =
      AppActivity::ActiveTime::Merge(active_time_1, active_time_3);
  EXPECT_FALSE(merged_time3.has_value());

  std::optional<AppActivity::ActiveTime> merged_time4 =
      AppActivity::ActiveTime::Merge(active_time_3, active_time_1);
  EXPECT_FALSE(merged_time4.has_value());

  std::optional<AppActivity::ActiveTime> merged_time5 =
      AppActivity::ActiveTime::Merge(active_time_2, active_time_3);
  EXPECT_TRUE(merged_time5.has_value());
  EXPECT_EQ(merged_time5->active_from(), time2 + delta);
  EXPECT_EQ(merged_time5->active_to(), time3);
}

// TODO(agawronska) : Add more tests for app activity.

}  // namespace app_time
}  // namespace ash
