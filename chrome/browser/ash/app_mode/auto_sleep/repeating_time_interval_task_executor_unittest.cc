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
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/native_timer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

constexpr char kTestTaskExecutorTag[] = "TestTaskExecutorTag";
constexpr char kTestTaskExecutorOtherTag[] = "TestTaskExecutorOtherTag";

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

struct IntervalTestFutures {
  base::test::TestFuture<void> interval_start;
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
    start_interval_wake_lock_count_ = 0;
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());
    policy::ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
  }

  void TearDown() override {
    chromeos::PowerManagerClient::Shutdown();
    policy::ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
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
      base::RepeatingClosure on_interval_start_callback,
      base::RepeatingClosure on_interval_end_future,
      const std::string& tag) {
    return std::make_unique<FakeRepeatingTimeIntervalTaskExecutor>(
        interval, on_interval_start_callback, on_interval_end_future, tag,
        task_environment_.GetMockClock());
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

  int WaitForActiveWakeLockCount() {
    base::test::TestFuture<int32_t> test_future;

    wake_lock_provider_.GetActiveWakeLocksForTests(
        device::mojom::WakeLockType::kPreventAppSuspension,
        test_future.GetCallback());

    return test_future.Take();
  }

  void OnIntervalStartForWakeLockTest(
      base::test::TestFuture<void>* on_interval_start_future) {
    task_environment_.RunUntilIdle();
    start_interval_wake_lock_count_ = WaitForActiveWakeLockCount();
    on_interval_start_future->SetValue();
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  int start_interval_wake_lock_count() {
    return start_interval_wake_lock_count_;
  }

 private:
  int start_interval_wake_lock_count_ = 0;
  device::TestWakeLockProvider wake_lock_provider_;
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
      CreateTestTaskExecutor(
          interval, future.interval_start.GetRepeatingCallback(),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);

  // Confirm that interval start callback is executed when the current time is
  // at the start of the interval.
  EXPECT_FALSE(future.interval_start.IsReady());
  FastForwardTimeTo(interval.start());
  task_executor->Start();
  EXPECT_TRUE(future.interval_start.WaitAndClear());

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
      CreateTestTaskExecutor(
          interval, future.interval_start.GetRepeatingCallback(),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);
  // Confirm that interval start callback is executed when the current time is
  // at the middle of the interval.
  auto delta = interval.start().GetDurationTo(interval.end()) / 2;
  EXPECT_FALSE(future.interval_start.IsReady());

  FastForwardTimeTo(interval.start());
  task_environment()->FastForwardBy(delta);
  task_executor->Start();
  EXPECT_TRUE(future.interval_start.WaitAndClear());

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
      CreateTestTaskExecutor(
          interval, future.interval_start.GetRepeatingCallback(),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);

  // Confirm that when you're outside an interval and then start the
  // timer, it schedules the timer for the future.
  EXPECT_FALSE(future.interval_start.IsReady());
  FastForwardTimeTo(interval.start(), -base::Minutes(5));
  task_executor->Start();
  // Run until idle to make sure that any native timer tasks are scheduled.
  task_environment()->RunUntilIdle();

  EXPECT_FALSE(future.interval_start.IsReady());

  FastForwardTimeTo(interval.start());
  EXPECT_TRUE(future.interval_start.WaitAndClear());

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
      CreateTestTaskExecutor(
          interval, future.interval_start.GetRepeatingCallback(),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);

  // Move time to before interval start so that test expectations are met and we
  // do not get multiple values in a test future that expects only one.
  FastForwardTimeTo(interval.start(), -base::Minutes(5));

  task_executor->Start();

  for (size_t i = 0; i < 3; i++) {
    FastForwardTimeTo(interval.start());
    EXPECT_TRUE(future.interval_start.WaitAndClear());

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
      CreateTestTaskExecutor(
          interval_1, future.interval_start.GetRepeatingCallback(),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);

  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor_2 =
      CreateTestTaskExecutor(interval_2,
                             future.interval_start.GetRepeatingCallback(),
                             future.interval_end.GetRepeatingCallback(),
                             kTestTaskExecutorOtherTag);

  FastForwardTimeTo(interval_1.start());
  task_executor_1->Start();
  task_executor_2->Start();

  EXPECT_TRUE(future.interval_start.WaitAndClear());
  FastForwardTimeTo(interval_1.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());

  // Do not need to fast forward the time, since the end of the first interval
  // overlaps with the start of the second.
  EXPECT_TRUE(future.interval_start.WaitAndClear());
  FastForwardTimeTo(interval_2.end());
  EXPECT_TRUE(future.interval_end.WaitAndClear());
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       TaskExecutorAcquiresAndReleasesWakeLock) {
  IntervalTestFutures future;

  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::FRIDAY,
                               base::Hours(12),  // 12:00 PM
                               DayOfWeek::SATURDAY,
                               base::Hours(8)  // 8:00 AM
      );

  // Bind to `OnIntervalStartForWakeLocks` in the test class so that we can
  // capture the no of wake locks before they are released.
  // Note: Unretained usage safe as the `RepeatingTimeIntervalTaskExecutorTest`
  // will always outlive the `task_executor`
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(
          interval,
          base::BindRepeating(&RepeatingTimeIntervalTaskExecutorTest::
                                  OnIntervalStartForWakeLockTest,
                              base::Unretained(this), &future.interval_start),
          future.interval_end.GetRepeatingCallback(), kTestTaskExecutorTag);

  // Confirm that interval start callback is executed when the timer is
  // started.
  FastForwardTimeTo(interval.start());
  EXPECT_EQ(WaitForActiveWakeLockCount(), 0);
  EXPECT_EQ(start_interval_wake_lock_count(), 0);
  task_executor->Start();

  // Run until idle so that the wake lock can be registered.
  task_environment()->RunUntilIdle();
  EXPECT_TRUE(future.interval_start.WaitAndClear());
  EXPECT_EQ(start_interval_wake_lock_count(), 1);
  EXPECT_EQ(WaitForActiveWakeLockCount(), 0);
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest, CallbacksNotCalledOnFailure) {
  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::TUESDAY,
                               base::Hours(8),  // 8:00 AM
                               DayOfWeek::SATURDAY,
                               base::Hours(8)  // 8:00 AM
      );
  chromeos::NativeTimer::ScopedFailureSimulatorForTesting
      scoped_failure_simulator;

  // Bind lambdas with `FAIL` which will ensure that if the callbacks get
  // called, the test will fail.
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval, base::BindRepeating([]() { FAIL(); }),
                             base::BindRepeating([]() { FAIL(); }),
                             kTestTaskExecutorTag);
  FastForwardTimeTo(interval.start());
  task_executor->Start();
  task_environment()->RunUntilIdle();
  FastForwardTimeTo(interval.end());
  task_environment()->RunUntilIdle();
}

TEST_F(RepeatingTimeIntervalTaskExecutorTest,
       CallbacksNotCalledOnFailureIntevalInFuture) {
  policy::WeeklyTimeInterval interval =
      CreateWeeklyTimeInterval(DayOfWeek::TUESDAY,
                               base::Hours(8),  // 8:00 AM
                               DayOfWeek::SATURDAY,
                               base::Hours(8)  // 8:00 AM
      );
  chromeos::NativeTimer::ScopedFailureSimulatorForTesting
      scoped_failure_simulator;

  // Bind lambdas with `FAIL` which will ensure that if the callbacks get
  // called, the test will fail.
  std::unique_ptr<FakeRepeatingTimeIntervalTaskExecutor> task_executor =
      CreateTestTaskExecutor(interval, base::BindRepeating([]() { FAIL(); }),
                             base::BindRepeating([]() { FAIL(); }),
                             kTestTaskExecutorTag);
  FastForwardTimeTo(interval.start(), -base::Minutes(5));
  task_executor->Start();
  task_environment()->RunUntilIdle();
  FastForwardTimeTo(interval.start());
  task_environment()->RunUntilIdle();
  FastForwardTimeTo(interval.end());
  task_environment()->RunUntilIdle();
}

}  // namespace ash
