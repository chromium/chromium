// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/weekly_interval_timer.h"

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

namespace {

policy::WeeklyTimeInterval CreateWeeklyTimeInterval(
    DayOfWeek start_day_of_week,
    const base::TimeDelta& start_time_of_day,
    DayOfWeek end_day_of_week,
    const base::TimeDelta& end_time_of_day) {
  policy::WeeklyTime start{start_day_of_week,
                           static_cast<int>(start_time_of_day.InMilliseconds()),
                           std::nullopt};
  policy::WeeklyTime end{end_day_of_week,
                         static_cast<int>(end_time_of_day.InMilliseconds()),
                         std::nullopt};
  policy::WeeklyTimeInterval interval{start, end};

  return interval;
}

WeeklyTimeInterval AnyInterval() {
  return CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(21),
                                  DayOfWeek::SATURDAY, base::Hours(6));
}

base::TimeDelta GetDuration(const policy::WeeklyTimeInterval& interval) {
  return interval.start().GetDurationTo(interval.end());
}

}  // namespace

class WeeklyIntervalTimerTest : public testing::Test {
 public:
  WeeklyIntervalTimerTest() = default;

  WeeklyIntervalTimerTest(WeeklyIntervalTimerTest&) = delete;
  WeeklyIntervalTimerTest& operator=(WeeklyIntervalTimerTest&) = delete;

  ~WeeklyIntervalTimerTest() override = default;

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

  std::unique_ptr<WeeklyIntervalTimer> CreateWeeklyIntervalTimer(
      const policy::WeeklyTimeInterval& interval,
      base::RepeatingCallback<void(base::TimeDelta)>
          on_interval_start_callback) {
    auto result =
        WeeklyIntervalTimer::Factory(task_environment().GetMockClock(),
                                     task_environment().GetMockTickClock())
            .Create(interval, on_interval_start_callback);

    // The timer could immediately schedule an async call to the callback,
    // so give this a chance to happen.
    task_environment().RunUntilIdle();
    return result;
  }

  base::test::TaskEnvironment& task_environment() { return task_environment_; }

 private:
  base::test::TaskEnvironment task_environment_{
      base::test::TaskEnvironment::MainThreadType::IO,
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
};

TEST_F(WeeklyIntervalTimerTest, TimerWorksWhenTimeFallsInCurrentInterval) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  // Confirm that interval start callback is executed when the timer is created
  // at the start of the interval.
  FastForwardTimeTo(interval.start());
  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(WeeklyIntervalTimerTest, TimerWorksWhenTimeFallsInsideInterval) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  // Confirm that interval start callback is executed when the timer is created
  // during the interval.
  const auto delta = GetDuration(interval) / 2;

  FastForwardTimeTo(interval.start(), /*delta=*/delta);
  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  const auto remaining_time_in_interval = GetDuration(interval) - delta;
  EXPECT_EQ(interval_start_future.Take(), remaining_time_in_interval);
}

TEST_F(WeeklyIntervalTimerTest, TimerRunsInTheFuture) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  // Confirm that when you're outside an interval and then start the
  // timer, it schedules the timer for the future.
  FastForwardTimeTo(interval.start(), -base::Minutes(5));

  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  EXPECT_FALSE(interval_start_future.IsReady());

  FastForwardTimeTo(interval.start());
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(WeeklyIntervalTimerTest, ShouldNotRunCallbackWhenCreatedAtIntervalEnd) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  // Start the test exactly at the end of the interval.
  FastForwardTimeTo(interval.end());

  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  // Timer should not trigger immediately.
  EXPECT_FALSE(interval_start_future.IsReady());

  // Timer should not trigger as long as we don't hit the interval start.
  FastForwardTimeTo(interval.start(), -base::Seconds(1));
  EXPECT_FALSE(interval_start_future.IsReady());

  // Timer should trigger when we hit the interval start.
  FastForwardTimeTo(interval.start());
  EXPECT_TRUE(interval_start_future.IsReady());
}

TEST_F(WeeklyIntervalTimerTest, TimerRunsEveryWeek) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  // The test starts before the interval.
  FastForwardTimeTo(interval.start(), -base::Minutes(5));

  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  for (size_t i = 0; i < 3; i++) {
    FastForwardTimeTo(interval.start());
    EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
  }
}

TEST_F(WeeklyIntervalTimerTest, TwoTimersWorkConcurrently) {
  base::test::TestFuture<base::TimeDelta> interval_1_start_future;
  base::test::TestFuture<base::TimeDelta> interval_2_start_future;

  const auto interval_1 =
      CreateWeeklyTimeInterval(DayOfWeek::WEDNESDAY, base::Hours(7),
                               DayOfWeek::WEDNESDAY, base::Hours(21));
  const auto interval_2 =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY, base::Hours(7),  //
                               DayOfWeek::FRIDAY, base::Hours(21));

  // The test starts before the first interval.
  FastForwardTimeTo(interval_1.start(), -base::Minutes(5));

  auto timer_1 = CreateWeeklyIntervalTimer(
      interval_1, interval_1_start_future.GetRepeatingCallback());
  auto timer_2 = CreateWeeklyIntervalTimer(
      interval_2, interval_2_start_future.GetRepeatingCallback());

  // Moving into the first interval should trigger the first callback.
  FastForwardTimeTo(interval_1.start());
  EXPECT_EQ(interval_1_start_future.Take(), GetDuration(interval_1));
  EXPECT_FALSE(interval_2_start_future.IsReady());

  // Moving into the second interval should trigger the second callback.
  FastForwardTimeTo(interval_2.start());
  EXPECT_FALSE(interval_1_start_future.IsReady());
  EXPECT_EQ(interval_2_start_future.Take(), GetDuration(interval_2));
}

TEST_F(WeeklyIntervalTimerTest, TimezoneChangesReprogramTimer) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"PST");

  FastForwardTimeTo(interval.start(), -base::Hours(8));

  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  // Change the time zone to CET which is 8 hours ahead of PST and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"CET");
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(WeeklyIntervalTimerTest, TimezoneChangesRestartTimer) {
  base::test::TestFuture<base::TimeDelta> interval_start_future;
  const auto interval = AnyInterval();

  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");

  FastForwardTimeTo(interval.start());
  auto timer = CreateWeeklyIntervalTimer(
      interval, interval_start_future.GetRepeatingCallback());

  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
  // Change the time zone to GMT+1 and confirm that
  // interval should start immediately as the timer should have been
  // reprogrammed.
  scoped_timezone_settings->SetTimezoneFromID(u"GMT+1");
  EXPECT_EQ(interval_start_future.Take(), GetDuration(interval));
}

TEST_F(WeeklyIntervalTimerTest,
       TimezoneChangeToSameTimezoneDoesNotRetriggerCallback) {
  auto scoped_timezone_settings =
      std::make_unique<system::ScopedTimezoneSettings>(u"GMT");
  const auto interval = AnyInterval();

  int interval_start_callback_count = 0;
  base::RepeatingCallback<void(base::TimeDelta)> interval_start_callback =
      base::BindRepeating(
          [](int& interval_start_callback_count, base::TimeDelta delta) {
            interval_start_callback_count++;
          },
          std::ref(interval_start_callback_count));

  FastForwardTimeTo(interval.start());

  auto timer = CreateWeeklyIntervalTimer(interval, interval_start_callback);
  EXPECT_EQ(interval_start_callback_count, 1);
  scoped_timezone_settings->SetTimezoneFromID(u"GMT");
  task_environment().RunUntilIdle();

  // Confirm that the interval doesn't start again when the callback is called
  // for the same timezone.
  EXPECT_EQ(interval_start_callback_count, 1);
  FastForwardTimeTo(interval.end());
  EXPECT_EQ(interval_start_callback_count, 1);
}

}  // namespace ash
