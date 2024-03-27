// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/fake_repeating_time_interval_task_executor.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/native_timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

struct IntervalTestFutures {
  base::test::TestFuture<base::TimeDelta> interval_start;
  base::test::TestFuture<void> interval_end;
};

}  // namespace

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

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
  }

  void FastForwardTimeTo(const policy::WeeklyTime& weekly_time,
                         base::TimeDelta delta = base::TimeDelta()) {
    base::Time current_time = task_environment_.GetMockClock()->Now();
    policy::WeeklyTime current_weekly_time =
        policy::WeeklyTime::GetLocalWeeklyTime(current_time);

    base::TimeDelta duration = current_weekly_time.GetDurationTo(weekly_time);
    task_environment_.FastForwardBy(duration + delta);
  }

  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> CreateTestTaskExecutor(
      const policy::WeeklyTimeInterval& interval,
      base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
      base::RepeatingClosure on_interval_end_future) {
    return std::make_unique<FakeRepeatingTimeIntervalTaskExecutor>(
        interval, on_interval_start_callback, on_interval_end_future,
        task_environment_.GetMockClock(), task_environment_.GetMockTickClock());
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

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TaskExecutorWorksWhenTimeFallsInCurrentInterval) {
  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::SATURDAY,
                               base::Hours(6)  // 6:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  // Confirm that interval start callback is executed when the current time is
  // at the start of the interval.
  EXPECT_FALSE(future.interval_start.IsReady());
  FastForwardTimeTo(interval.start());
  task_executor->ScheduleTimer();
  EXPECT_TRUE(!future.interval_start.Take().is_zero());

  // Confirm that interval end callback is executed when the timer is
  // finished.
  EXPECT_FALSE(future.interval_end.IsReady());
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TaskExecutorWorksWhenTimeFallsInMiddleOfInterval) {
  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::SATURDAY,
                               base::Hours(6)  // 6:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());
  // Confirm that interval start callback is executed when the current time is
  // at the middle of the interval.
  auto delta = interval.start().GetDurationTo(interval.end()) / 2;
  EXPECT_FALSE(future.interval_start.IsReady());

  FastForwardTimeTo(interval.start());
  task_environment()->FastForwardBy(delta);
  task_executor->ScheduleTimer();
  EXPECT_TRUE(!future.interval_start.Take().is_zero());

  EXPECT_FALSE(future.interval_end.IsReady());
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TaskExecutorRunsInTheFuture) {
  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::SATURDAY,
                               base::Hours(6)  // 6:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  // Confirm that when you're outside an interval and then start the
  // timer, it schedules the timer for the future.
  EXPECT_FALSE(future.interval_start.IsReady());
  FastForwardTimeTo(interval.start(), -base::Minutes(5));
  task_executor->ScheduleTimer();
  // Run until idle to make sure that any native timer tasks are scheduled.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(future.interval_start.IsReady());

  FastForwardTimeTo(interval.start());
  EXPECT_TRUE(!future.interval_start.Take().is_zero());

  EXPECT_FALSE(future.interval_end.IsReady());
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TaskExecutorRunsEveryWeek) {
  IntervalTestFutures future;
  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY,
                               base::Hours(7),  // 7:00 AM
                               DayOfWeek::WEDNESDAY,
                               base::Hours(21)  // 9:00 PM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  // Move time to before interval start so that test expectations are met and we
  // do not get multiple values in a test future that expects only one.
  FastForwardTimeTo(interval.start(), -base::Minutes(5));

  task_executor->ScheduleTimer();

  for (size_t i = 0; i < 3; i++) {
    FastForwardTimeTo(interval.start());
    EXPECT_TRUE(!future.interval_start.Take().is_zero());

    EXPECT_FALSE(future.interval_end.IsReady());
    FastForwardTimeTo(interval.end());
    EXPECT_TRUE(future.interval_end.WaitAndClear());
  }
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TwoTaskExecutorsWorkConcurrently) {
  IntervalTestFutures future;
  policy::WeeklyTimeInterval interval_1 =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY,
                               base::Hours(7),  // 7:00 AM
                               DayOfWeek::WEDNESDAY,
                               base::Hours(21)  // 9:00 PM
      );

  policy::WeeklyTimeInterval interval_2 =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::WEDNESDAY,
                               base::Hours(7)  // 7:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor_1 =
      CreateTestTaskExecutor(interval_1,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor_2 =
      CreateTestTaskExecutor(interval_2,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  FastForwardTimeTo(interval_1.start());
  task_executor_1->ScheduleTimer();
  task_executor_2->ScheduleTimer();

  EXPECT_TRUE(!future.interval_start.Take().is_zero());
  FastForwardTimeTo(interval_1.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());

  // Do not need to fast forward the time, since the end of the first interval
  // overlaps with the start of the second.
  EXPECT_TRUE(!future.interval_start.Take().is_zero());
  FastForwardTimeTo(interval_2.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TimezoneChangesReprogramTimer) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"PST");

  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY,
                               base::Hours(12),  // 12:00 PM
                               DayOfWeek::SATURDAY,
                               base::Hours(8)  // 8:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  FastForwardTimeTo(interval.start(), -base::Hours(8));
  task_executor->ScheduleTimer();
  // Change the time zone to CET which is 8 hours ahead of PST and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"CET");
  // Confirm that the timer is executed as you're in a timezone where the
  // current time is already inside the interval.
  EXPECT_TRUE(!future.interval_start.Take().is_zero());
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, TimezoneChangesRestartTimer) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");

  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::MONDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::TUESDAY,
                               base::Hours(8)  // 8:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback());

  FastForwardTimeTo(interval.start());
  task_executor->ScheduleTimer();

  EXPECT_TRUE(!future.interval_start.Take().is_zero());
  // Change the time zone to GMT+1 and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT+1");
  EXPECT_TRUE(future.interval_end.WaitAndClear());

  // Confirm that the timer is executed as you're in a timezone where the
  // current time is already inside the interval.
  EXPECT_TRUE(!future.interval_start.Take().is_zero());
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TimezoneChangeToSameTimezoneDoesNotRestartTimer) {
  base::test::TestFuture<void> interval_end_future;
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");
  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::MONDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::TUESDAY,
                               base::Hours(8)  // 8:00 AM
      );
  int interval_start_callback_count = 0;
  base::RepeatingCallback<void(base::TimeDelta)> interval_start_callback =
      base::BindRepeating(
          [](int& interval_start_callback_count, base::TimeDelta delta) {
            interval_start_callback_count++;
          },
          std::ref(interval_start_callback_count));

  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval, interval_start_callback,
                             interval_end_future.GetRepeatingCallback());

  FastForwardTimeTo(interval.start());
  EXPECT_EQ(interval_start_callback_count, 0);
  task_executor->ScheduleTimer();
  EXPECT_EQ(interval_start_callback_count, 1);
  scoped_timezone_settings->SetTimezoneFromID(u"GMT");
  task_environment()->RunUntilIdle();

  // Confirm that the interval doesn't start again when the callback is called
  // for the same timezone.
  EXPECT_EQ(interval_start_callback_count, 1);
  FastForwardTimeTo(interval.end());
  EXPECT_TRUE(interval_end_future.WaitAndClear());
  EXPECT_EQ(interval_start_callback_count, 1);
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TimezoneChangesSendsNotifyUserNotification) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");

  base::test::TestFuture<void> user_activity_future;
  chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
      user_activity_future.GetRepeatingCallback());

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::MONDAY,
                               base::Hours(21),  // 9:00 PM
                               DayOfWeek::TUESDAY,
                               base::Hours(8)  // 8:00 AM
      );
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval,
                             base::BindRepeating([](base::TimeDelta delta) {}),
                             base::BindRepeating([]() {}));

  FastForwardTimeTo(interval.start());
  task_executor->ScheduleTimer();
  // Confirm that changing the timezone fires a user activity callback to cancel
  // any pending suspend calls.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT-1");
  EXPECT_TRUE(user_activity_future.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       ChangingTimeZoneWithoutStartingExecutorIsNoOp) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"PST");

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::MONDAY,
                               base::Hours(12),  // 12:00 PM
                               DayOfWeek::TUESDAY,
                               base::Hours(8)  // 8:00 AM
      );
  // Bind lambdas with `FAIL` which will ensure that if the callbacks get
  // called, the test will fail.
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(
          interval, base::BindRepeating([](base::TimeDelta) { FAIL(); }),
          base::BindRepeating([]() { FAIL(); }));
  FastForwardTimeTo(interval.start(), -base::Hours(8));
  scoped_timezone_settings->SetTimezoneFromID(u"CET");
  FastForwardTimeTo(interval.start());
  task_environment()->RunUntilIdle();
  FastForwardTimeTo(interval.end());
  task_environment()->RunUntilIdle();
}

}  // namespace ash
