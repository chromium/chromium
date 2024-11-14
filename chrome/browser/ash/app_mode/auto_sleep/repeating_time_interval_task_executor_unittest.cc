// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

class RepeatingTimeIntervalTaskExecutorTest : public testing::Test {
 public:
  RepeatingTimeIntervalTaskExecutorTest() = default;

  RepeatingTimeIntervalTaskExecutorTest(
      RepeatingTimeIntervalTaskExecutorTest&) = delete;
  RepeatingTimeIntervalTaskExecutorTest& operator=(
      RepeatingTimeIntervalTaskExecutorTest&) = delete;

  ~RepeatingTimeIntervalTaskExecutorTest() override = default;

  // testing::Test:
  void SetUp() override {
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
  }

  void TearDown() override { chromeos::PowerManagerClient::Shutdown(); }

  void FastForwardTimeTo(const policy::WeeklyTime& weekly_time,
                         base::TimeDelta delta = base::TimeDelta()) {
    base::Time current_time = task_environment().GetMockClock()->Now();
    policy::WeeklyTime current_weekly_time =
        policy::WeeklyTime::GetLocalWeeklyTime(current_time);

    base::TimeDelta duration = current_weekly_time.GetDurationTo(weekly_time);
    task_environment().FastForwardBy(duration + delta);
  }

  std::unique_ptr<RepeatingTimeIntervalTaskExecutor> CreateTestTaskExecutor(
      const policy::WeeklyTimeInterval& interval,
      base::RepeatingCallback<void(base::TimeDelta)>
          on_interval_start_callback) {
    return RepeatingTimeIntervalTaskExecutor::Factory(
               task_environment().GetMockClock(),
               task_environment().GetMockTickClock())
        .Create(interval, on_interval_start_callback);
  }

  policy::WeeklyTimeInterval CreateWeeklyTimeInterval(
      DayOfWeek start_day_of_week,
      const base::TimeDelta& start_time_of_day,
      DayOfWeek end_day_of_week,
      const base::TimeDelta& end_time_of_day) {
    policy::WeeklyTime start{
        start_day_of_week, static_cast<int>(start_time_of_day.InMilliseconds()),
        std::nullopt};
    policy::WeeklyTime end{end_day_of_week,
                           static_cast<int>(end_time_of_day.InMilliseconds()),
                           std::nullopt};
    policy::WeeklyTimeInterval interval{start, end};

    return interval;
  }

  base::TimeDelta GetDuration(const policy::WeeklyTimeInterval& interval) {
    return interval.start().GetDurationTo(interval.end());
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TaskExecutorWorksWhenTimeFallsInCurrentInterval) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(21),
                                           DayOfWeek::SATURDAY, base::Hours(6));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  // Confirm that interval start callback is executed when the current time is
  // at the start of the interval.
  EXPECT_FALSE(interval_start_future.IsReady());
  FastForwardTimeTo(interval.start());
  task_executor->ScheduleTimer();
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TaskExecutorWorksWhenTimeFallsInsideInterval) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(21),
                                           DayOfWeek::SATURDAY, base::Hours(6));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());
  // Confirm that interval start callback is executed when the current time is
  // inside the interval.
  auto delta = GetDuration(interval) / 2;
  EXPECT_FALSE(interval_start_future.IsReady());

  FastForwardTimeTo(interval.start());
  task_environment().FastForwardBy(delta);
  task_executor->ScheduleTimer();
  const auto remaining_time_in_interval = GetDuration(interval) - delta;
  EXPECT_EQ(interval_start_future.Take(), remaining_time_in_interval);
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TaskExecutorRunsInTheFuture) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(21),
                                           DayOfWeek::SATURDAY, base::Hours(6));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  // Confirm that when you're outside an interval and then start the
  // timer, it schedules the timer for the future.
  EXPECT_FALSE(interval_start_future.IsReady());
  FastForwardTimeTo(interval.start(), -base::Minutes(5));
  task_executor->ScheduleTimer();
  // Run until idle to make sure that any native timer tasks are scheduled.
  task_environment().RunUntilIdle();

  EXPECT_FALSE(interval_start_future.IsReady());

  FastForwardTimeTo(interval.start());
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       ShouldNotRunCallbackWhenCreatedAtIntervalEnd) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  auto interval = CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(21),
                                           DayOfWeek::SATURDAY, base::Hours(6));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  // Start the test exactly at the end of the interval.
  FastForwardTimeTo(interval.end());

  task_executor->ScheduleTimer();
  task_environment().RunUntilIdle();

  EXPECT_FALSE(interval_start_future.IsReady());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TaskExecutorRunsEveryWeek) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  auto interval =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY, base::Hours(7),
                               DayOfWeek::WEDNESDAY, base::Hours(21));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  // Move time to before interval start so that test expectations are met and we
  // do not get multiple values in a test future that expects only one.
  FastForwardTimeTo(interval.start(), -base::Minutes(5));

  task_executor->ScheduleTimer();

  for (size_t i = 0; i < 3; i++) {
    FastForwardTimeTo(interval.start());
    EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
  }
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TwoTaskExecutorsWorkConcurrently) {
  base::test::TestFuture<base::TimeDelta> interval_1_start_future;
  auto interval_1 =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY, base::Hours(7),
                               DayOfWeek::WEDNESDAY, base::Hours(21));
  auto task_executor_1 = CreateTestTaskExecutor(
      interval_1, interval_1_start_future.GetRepeatingCallback());

  base::test::TestFuture<base::TimeDelta> interval_2_start_future;
  auto interval_2 = CreateWeeklyTimeInterval(
      DayOfWeek::FRIDAY, base::Hours(7), DayOfWeek::FRIDAY, base::Hours(21));
  auto task_executor_2 = CreateTestTaskExecutor(
      interval_2, interval_2_start_future.GetRepeatingCallback());

  // The test starts before the first interval.
  FastForwardTimeTo(interval_1.start(), -base::Minutes(5));

  task_executor_1->ScheduleTimer();
  task_executor_2->ScheduleTimer();

  // Moving into the first interval should trigger the first callback.
  FastForwardTimeTo(interval_1.start());
  EXPECT_EQ(interval_1_start_future.Take(), GetDuration(interval_1));
  EXPECT_FALSE(interval_2_start_future.IsReady());

  // Moving into the second interval should trigger the second callback.
  FastForwardTimeTo(interval_2.start());
  EXPECT_FALSE(interval_1_start_future.IsReady());
  EXPECT_EQ(interval_2_start_future.Take(), GetDuration(interval_2));
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TimezoneChangesReprogramTimer) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"PST");

  base::test::TestFuture<base::TimeDelta> interval_start_future;

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(12),
                                           DayOfWeek::SATURDAY, base::Hours(8));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  FastForwardTimeTo(interval.start(), -base::Hours(8));
  task_executor->ScheduleTimer();
  // Change the time zone to CET which is 8 hours ahead of PST and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"CET");
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TimezoneChangesRestartTimer) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");

  base::test::TestFuture<base::TimeDelta> interval_start_future;

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::MONDAY, base::Hours(21),
                                           DayOfWeek::TUESDAY, base::Hours(8));
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  FastForwardTimeTo(interval.start());
  task_executor->ScheduleTimer();

  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
  // Change the time zone to GMT+1 and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT+1");
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TimezoneChangeToSameTimezoneDoesNotRestartTimer) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");
  auto interval = CreateWeeklyTimeInterval(DayOfWeek::MONDAY, base::Hours(21),
                                           DayOfWeek::TUESDAY, base::Hours(8));
  int interval_start_callback_count = 0;
  base::RepeatingCallback<void(base::TimeDelta)> interval_start_callback =
      base::BindRepeating(
          [](int& interval_start_callback_count, base::TimeDelta delta) {
            interval_start_callback_count++;
          },
          std::ref(interval_start_callback_count));

  auto task_executor =
      CreateTestTaskExecutor(interval, interval_start_callback);

  FastForwardTimeTo(interval.start());
  EXPECT_EQ(interval_start_callback_count, 0);
  task_executor->ScheduleTimer();
  EXPECT_EQ(interval_start_callback_count, 1);
  scoped_timezone_settings->SetTimezoneFromID(u"GMT");
  task_environment().RunUntilIdle();

  // Confirm that the interval doesn't start again when the callback is called
  // for the same timezone.
  EXPECT_EQ(interval_start_callback_count, 1);
  FastForwardTimeTo(interval.end());
  EXPECT_EQ(interval_start_callback_count, 1);
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       ChangingTimeZoneWithoutStartingExecutorIsNoOp) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"PST");

  auto interval = CreateWeeklyTimeInterval(DayOfWeek::MONDAY, base::Hours(12),
                                           DayOfWeek::TUESDAY, base::Hours(8));

  base::test::TestFuture<base::TimeDelta> interval_start_future;
  auto task_executor = CreateTestTaskExecutor(
      interval, interval_start_future.GetRepeatingCallback());

  FastForwardTimeTo(interval.start(), -base::Hours(8));
  scoped_timezone_settings->SetTimezoneFromID(u"CET");
  FastForwardTimeTo(interval.start());

  EXPECT_FALSE(interval_start_future.IsReady());
}

}  // namespace ash
