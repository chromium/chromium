// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"

#include <memory>
#include <optional>

#include "ash/constants/ash_switches.h"
#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/time/time.h"
#include "base/time/time_delta_from_string.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

namespace {

constexpr base::TimeDelta kDefaultGracePeriod = base::Hours(1);

base::Time TimeFromUtcString(const char* time) {
  base::Time delayult;
  bool success = base::Time::FromUTCString(time, &delayult);
  CHECK(success);
  return delayult;
}

std::unique_ptr<icu::TimeZone> GetUtcTimeZone() {
  return base::WrapUnique(
      icu::TimeZone::createTimeZone(icu::UnicodeString::fromUTF8("UTC")));
}

}  // namespace

using scheduled_task_util::CalculateNextScheduledTaskTimerDelay;

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledInSameDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 12;
  data.minute = 52;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("3h19m"));
}

TEST(ScheduledTaskUtilTest, TaskShouldBeDelayedIfTimesMatch) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 12;
  data.minute = 52;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 12:52"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("24h"));
}

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledNextDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("23h19m"));
}

TEST(ScheduledTaskUtilTest,
     DailyTaskShouldBeScheduledNextDayAcrossMonthBoundary) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::Hours(23) + base::Minutes(19));
}

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledNextDayBeforeLeapDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  const base::Time current_time = TimeFromUtcString("Feb 28 1964, 09:33");

  const std::optional<base::TimeDelta> delay =
      CalculateNextScheduledTaskTimerDelay(data, current_time,
                                           *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::Hours(23) + base::Minutes(19));
}

TEST(ScheduledTaskUtilTest, DailyTaskShouldBeScheduledNextDayOnLeapDay) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kDaily;
  data.hour = 8;
  data.minute = 52;

  const base::Time current_time = TimeFromUtcString("Feb 29 1964, 09:33");

  const std::optional<base::TimeDelta> delay =
      CalculateNextScheduledTaskTimerDelay(data, current_time,
                                           *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("23h19m"));
}

TEST(ScheduledTaskUtilTest, WeeklyTaskShouldBeScheduled) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kWeekly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_week = UCAL_TUESDAY;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Sunday Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("50h19m"));  // 2d2h19m
}

TEST(ScheduledTaskUtilTest, MonthlyTaskShouldBeScheduled) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_month = 23;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 3 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("482h19m"));  // 20d2h19m
}

TEST(ScheduledTaskUtilTest, MonthlyTaskShouldBeScheduledForNextMonth) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 11;
  data.minute = 52;
  data.day_of_month = 23;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("554h19m"));  // 23d2h19m
}

TEST(ScheduledTaskUtilTest,
     MonthlyTaskShouldBeScheduledForEndOfMonthIfMonthShorter) {
  ScheduledTaskExecutor::ScheduledTaskData data;
  data.frequency = ScheduledTaskExecutor::Frequency::kMonthly;
  data.hour = 8;
  data.minute = 52;
  data.day_of_month = 31;

  std::optional<base::TimeDelta> delay = CalculateNextScheduledTaskTimerDelay(
      data, TimeFromUtcString("Sunday Jan 31 2021, 09:33"), *GetUtcTimeZone());

  ASSERT_TRUE(delay.has_value());
  EXPECT_EQ(delay.value(), base::TimeDeltaFromString("671h19m"));  // 27d23h19m
}

TEST(ScheduledTaskUtilTest, ParsesEmptyGracePeriodSwitch) {
  base::test::ScopedCommandLine command_line;

  // Check that returns default with empty command line.
  EXPECT_EQ(scheduled_task_util::GetScheduledRebootGracePeriod(),
            kDefaultGracePeriod);

  // Check that returns default with empty switch.
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting, "");
  EXPECT_EQ(scheduled_task_util::GetScheduledRebootGracePeriod(),
            kDefaultGracePeriod);
}

TEST(ScheduledTaskUtilTest, ParsesNonNumbericGracePeriodSwitch) {
  base::test::ScopedCommandLine command_line;

  // Check that returns default with incorrect value.
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting,
      "one-hundred");
  EXPECT_EQ(scheduled_task_util::GetScheduledRebootGracePeriod(),
            kDefaultGracePeriod);
}

TEST(ScheduledTaskUtilTest, ParsesNegativeGracePeriodSwitch) {
  base::test::ScopedCommandLine command_line;

  // Check that returns default with negative value.
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting, "-100");
  EXPECT_EQ(scheduled_task_util::GetScheduledRebootGracePeriod(),
            kDefaultGracePeriod);
}

TEST(ScheduledTaskUtilTest, ParsesValidGracePeriodSwitch) {
  base::test::ScopedCommandLine command_line;

  // Check that returns default with negative value.
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting, "100");
  EXPECT_EQ(scheduled_task_util::GetScheduledRebootGracePeriod(),
            base::Seconds(100));
}

TEST(ScheduledTaskUtilTest, SkipsRebootWithinGracePeriod) {
  constexpr base::TimeDelta kGracePeriod = base::Hours(1);

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting,
      base::NumberToString(kGracePeriod.InSeconds()));

  const base::Time boot_time;

  // Check that allows reboot outside of grace time period.
  const base::Time reboot_time_outside_grace_time =
      boot_time + kGracePeriod + base::Seconds(1);
  EXPECT_FALSE(scheduled_task_util::ShouldSkipRebootDueToGracePeriod(
      boot_time, reboot_time_outside_grace_time));
}

TEST(ScheduledTaskUtilTest, DoesNotSkipRebootWithinGracePeriod) {
  constexpr base::TimeDelta kGracePeriod = base::Hours(1);

  base::test::ScopedCommandLine command_line;
  command_line.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kScheduledRebootGracePeriodInSecondsForTesting,
      base::NumberToString(kGracePeriod.InSeconds()));

  const base::Time boot_time;

  // Check that does not allow reboot inside grace time period.
  const base::Time reboot_time_within_grace_time = boot_time + kGracePeriod;
  EXPECT_TRUE(scheduled_task_util::ShouldSkipRebootDueToGracePeriod(
      boot_time, reboot_time_within_grace_time));
}

}  // namespace policy
