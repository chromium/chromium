// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <cstddef>
#include <functional>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/browser/ash/app_mode/auto_sleep/fake_repeating_time_interval_task_executor.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

base::TimeDelta GetDuration(const base::Time& start,
                            const policy::WeeklyTime& end) {
  policy::WeeklyTime start_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(start);
  return start_weekly_time.GetDurationTo(end);
}

// Scoped test helper that is responsible for the following things:
// - Mocking time by setting up the dependencies on construction and cleaning
// them up to prevent dangling pointers.
// - Providing utility methods that allow controlling the time.
//
// This should be created inside the actual test so that we do not get
// stuck because of RunLoops within the browser test initialization code.
//
// Note: This is a scoped mock time class because it overrides the current task
// runner with a mock time task runner. As a consequence of this you have to
// drive the task runner yourself. This is possible to do via using the
// `FastForwardTimeTo` and the `task_runner()` methods in the class.
class ScopedMockTimeScheduledSuspendTestHelper {
 public:
  ScopedMockTimeScheduledSuspendTestHelper() {
    DeviceWeeklyScheduledSuspendController* controller =
        KioskController::Get()
            .GetKioskSystemSession()
            ->device_weekly_scheduled_suspend_controller_for_testing();

    controller->SetTaskExecutorFactoryForTesting(
        std::make_unique<FakeRepeatingTimeIntervalTaskExecutor::Factory>(
            task_runner_->GetMockClock(), task_runner_->GetMockTickClock()));
    controller->SetClockForTesting(task_runner_->GetMockClock());
  }

  ~ScopedMockTimeScheduledSuspendTestHelper() {
    DeviceWeeklyScheduledSuspendController* controller =
        KioskController::Get()
            .GetKioskSystemSession()
            ->device_weekly_scheduled_suspend_controller_for_testing();

    controller->SetTaskExecutorFactoryForTesting(nullptr);
    controller->SetClockForTesting(nullptr);
    // Clear the policy so that we don't get dangling task executors.
    g_browser_process->local_state()->SetList(
        prefs::kDeviceWeeklyScheduledSuspend, {});
  }

  void FastForwardTimeTo(const policy::WeeklyTime& weekly_time) {
    auto current_time = task_runner_->GetMockClock()->Now();
    auto duration = GetDuration(current_time, weekly_time);
    task_runner_->FastForwardBy(duration);
  }

  base::TestMockTimeTaskRunner* task_runner() {
    return task_runner_.task_runner();
  }

 private:
  base::ScopedMockTimeMessageLoopTaskRunner task_runner_;
};

}  // namespace

class DeviceWeeklyScheduledSuspendControllerTest : public WebKioskBaseTest {
 public:
  // WebKioskBaseTest:
  void SetUpOnMainThread() override {
    WebKioskBaseTest::SetUpOnMainThread();
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::BindRepeating([](int& count) { ++count; },
                            std::ref(user_activity_calls_)));

    InitializeRegularOnlineKiosk();
    ASSERT_TRUE(KioskController::Get().GetKioskSystemSession());
  }

  void TearDownOnMainThread() override {
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::NullCallback());
    WebKioskBaseTest::TearDownOnMainThread();
  }

  // Updates the policy preferences which in turn trigger the pref observers in
  // the controller.
  void UpdatePolicyPref(base::Value::List schedule_list) {
    g_browser_process->local_state()->SetList(
        prefs::kDeviceWeeklyScheduledSuspend, std::move(schedule_list));
  }

  void SimulateResumeSuspend() {
    chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  }

  int user_activity_calls() { return user_activity_calls_; }

 private:
  int user_activity_calls_ = 0;
};

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendControllerExistOnKioskStartUp) {
  ASSERT_TRUE(KioskController::Get()
                  .GetKioskSystemSession()
                  ->device_weekly_scheduled_suspend_controller_for_testing());
}

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendAndWakeTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(user_activity_calls(), 0);
  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::MONDAY, base::Hours(0), DayOfWeek::MONDAY, base::Hours(9));
  auto intervals = policy_builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  UpdatePolicyPref(policy_builder.GetAsPrefValue());

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(user_activity_calls(), 0);

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  helper.FastForwardTimeTo(intervals[0]->end());

  SimulateResumeSuspend();

  EXPECT_EQ(user_activity_calls(), 1);
}

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       MultipleSuspendAndWakeTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(user_activity_calls(), 0);
  auto builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder()
          .AddWeeklySuspendInterval(DayOfWeek::MONDAY, base::Hours(0),
                                    DayOfWeek::MONDAY, base::Hours(9))
          .AddWeeklySuspendInterval(DayOfWeek::TUESDAY, base::Hours(0),
                                    DayOfWeek::TUESDAY, base::Hours(9))
          .AddWeeklySuspendInterval(DayOfWeek::WEDNESDAY, base::Hours(0),
                                    DayOfWeek::WEDNESDAY, base::Hours(9));
  auto intervals = builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  UpdatePolicyPref(builder.GetAsPrefValue());

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  helper.FastForwardTimeTo(intervals[0]->end());
  SimulateResumeSuspend();
  EXPECT_EQ(user_activity_calls(), 1);

  helper.FastForwardTimeTo(intervals[1]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 2);

  helper.FastForwardTimeTo(intervals[1]->end());
  SimulateResumeSuspend();
  EXPECT_EQ(user_activity_calls(), 2);

  helper.FastForwardTimeTo(intervals[2]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 3);

  helper.FastForwardTimeTo(intervals[2]->end());
  SimulateResumeSuspend();
  EXPECT_EQ(user_activity_calls(), 3);

  EXPECT_EQ(power_client->num_request_suspend_calls(),
            static_cast<int>(intervals.size()));
  EXPECT_EQ(user_activity_calls(), static_cast<int>(intervals.size()));
}

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       ManualWakeDuringIntervalClearsState) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::TUESDAY, base::Hours(0), DayOfWeek::TUESDAY,
          base::Hours(9));
  auto intervals = policy_builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  UpdatePolicyPref(policy_builder.GetAsPrefValue());

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);

  duration =
      GetDuration(clock->Now(), intervals[0]->start()) + base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  // Simulate user waking up the device during the suspend by calling
  // `SendSuspendDone`.
  power_client->SendSuspendDone();

  helper.FastForwardTimeTo(intervals[0]->end());

  // Confirm that subsequent resume events will not cause
  // unnecessary user activity calls to wake the device when we are at the end
  // of the interval.
  SimulateResumeSuspend();

  EXPECT_EQ(user_activity_calls(), 0);
}

IN_PROC_BROWSER_TEST_F(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendAndWakeRepeatsEveryWeekTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(user_activity_calls(), 0);
  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::SATURDAY, base::Hours(0), DayOfWeek::SATURDAY,
          base::Hours(9));
  auto intervals = policy_builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  UpdatePolicyPref(policy_builder.GetAsPrefValue());

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(user_activity_calls(), 0);

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  // Resume the device before the end of the interval.
  power_client->SendSuspendDone();

  // Confirm that the device calls request suspend again in a week.
  helper.task_runner()->FastForwardBy(base::Days(7));
  EXPECT_EQ(power_client->num_request_suspend_calls(), 2);
}

}  // namespace ash
