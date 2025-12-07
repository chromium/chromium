// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_controller.h"

#include <cstddef>
#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "base/test/bind.h"
#include "base/test/scoped_mock_time_message_loop_task_runner.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/app_mode/auto_sleep/device_weekly_scheduled_suspend_test_policy_builder.h"
#include "chrome/browser/ash/app_mode/auto_sleep/weekly_interval_timer.h"
#include "chrome/browser/ash/app_mode/kiosk_controller.h"
#include "chrome/browser/ash/app_mode/kiosk_system_session.h"
#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

using kiosk::test::WaitKioskLaunched;

namespace {

using DayOfWeek = DeviceWeeklyScheduledSuspendTestPolicyBuilder::DayOfWeek;

base::TimeDelta GetDuration(const base::Time& start,
                            const policy::WeeklyTime& end) {
  policy::WeeklyTime start_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(start);
  return start_weekly_time.GetDurationTo(end);
}

// Sets `schedule_list` as prefs for `kDeviceWeeklyScheduledSuspend` in local
// state, simulating a policy change.
void SetPrefInLocalState(base::Value::List schedule_list) {
  g_browser_process->local_state()->SetList(
      prefs::kDeviceWeeklyScheduledSuspend, std::move(schedule_list));
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

    controller->SetWeeklyIntervalTimerFactoryForTesting(
        std::make_unique<WeeklyIntervalTimer::Factory>(
            task_runner_->GetMockClock(), task_runner_->GetMockTickClock()));
    controller->SetClockForTesting(task_runner_->GetMockClock());
  }

  ~ScopedMockTimeScheduledSuspendTestHelper() {
    DeviceWeeklyScheduledSuspendController* controller =
        KioskController::Get()
            .GetKioskSystemSession()
            ->device_weekly_scheduled_suspend_controller_for_testing();

    controller->SetWeeklyIntervalTimerFactoryForTesting(nullptr);
    controller->SetClockForTesting(nullptr);
    // Clear the policy so that we don't get dangling timers.
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

// Helper mixin to encapsulate `FakePowerManagerClient` setup and functionality.
class FakePowerManagerMixin : public InProcessBrowserTestMixin {
 public:
  explicit FakePowerManagerMixin(InProcessBrowserTestMixinHost* host)
      : InProcessBrowserTestMixin(host) {}
  FakePowerManagerMixin(const FakePowerManagerMixin&) = delete;
  FakePowerManagerMixin(FakePowerManagerMixin&&) = delete;
  ~FakePowerManagerMixin() override = default;

  void SetUpOnMainThread() override {
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::BindLambdaForTesting([this] { ++user_activity_calls_; }));
  }

  void TearDownOnMainThread() override {
    chromeos::FakePowerManagerClient::Get()->set_user_activity_callback(
        base::NullCallback());
  }

  void SimulateResumeSuspend() {
    chromeos::FakePowerManagerClient::Get()->SendDarkSuspendImminent();
  }

  int user_activity_calls() const { return user_activity_calls_; }

 private:
  int user_activity_calls_ = 0;
};

}  // namespace

class DeviceWeeklyScheduledSuspendControllerTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<KioskMixin::Config> {
 public:
  DeviceWeeklyScheduledSuspendControllerTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }
  DeviceWeeklyScheduledSuspendControllerTest(
      const DeviceWeeklyScheduledSuspendControllerTest&) = delete;
  DeviceWeeklyScheduledSuspendControllerTest(
      DeviceWeeklyScheduledSuspendControllerTest&&) = delete;
  ~DeviceWeeklyScheduledSuspendControllerTest() override = default;

  void SetUpOnMainThread() override {
    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
    ASSERT_TRUE(WaitKioskLaunched());
  }

  FakePowerManagerMixin power_manager_{&mixin_host_};
  KioskMixin kiosk_{&mixin_host_,
                    /*cached_configuration=*/GetParam()};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendControllerExistOnKioskStartUp) {
  ASSERT_TRUE(KioskController::Get()
                  .GetKioskSystemSession()
                  ->device_weekly_scheduled_suspend_controller_for_testing());
}

IN_PROC_BROWSER_TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendAndWakeTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(power_manager_.user_activity_calls(), 0);

  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::MONDAY, base::Hours(0), DayOfWeek::MONDAY, base::Hours(9));
  auto intervals = policy_builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  SetPrefInLocalState(policy_builder.GetAsPrefValue());

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(power_manager_.user_activity_calls(), 0);

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  helper.FastForwardTimeTo(intervals[0]->end());

  power_manager_.SimulateResumeSuspend();

  EXPECT_EQ(power_manager_.user_activity_calls(), 1);
}

IN_PROC_BROWSER_TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
                       MultipleSuspendAndWakeTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(power_manager_.user_activity_calls(), 0);
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
  SetPrefInLocalState(builder.GetAsPrefValue());

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  helper.FastForwardTimeTo(intervals[0]->end());
  power_manager_.SimulateResumeSuspend();
  EXPECT_EQ(power_manager_.user_activity_calls(), 1);

  helper.FastForwardTimeTo(intervals[1]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 2);

  helper.FastForwardTimeTo(intervals[1]->end());
  power_manager_.SimulateResumeSuspend();
  EXPECT_EQ(power_manager_.user_activity_calls(), 2);

  helper.FastForwardTimeTo(intervals[2]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 3);

  helper.FastForwardTimeTo(intervals[2]->end());
  power_manager_.SimulateResumeSuspend();
  EXPECT_EQ(power_manager_.user_activity_calls(), 3);

  EXPECT_EQ(power_client->num_request_suspend_calls(),
            static_cast<int>(intervals.size()));
  EXPECT_EQ(power_manager_.user_activity_calls(),
            static_cast<int>(intervals.size()));
}

IN_PROC_BROWSER_TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
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

  SetPrefInLocalState(policy_builder.GetAsPrefValue());

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
  power_manager_.SimulateResumeSuspend();

  EXPECT_EQ(power_manager_.user_activity_calls(), 0);
}

IN_PROC_BROWSER_TEST_P(DeviceWeeklyScheduledSuspendControllerTest,
                       SuspendAndWakeRepeatsEveryWeekTest) {
  ScopedMockTimeScheduledSuspendTestHelper helper;

  auto* power_client = chromeos::FakePowerManagerClient::Get();
  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(power_manager_.user_activity_calls(), 0);
  auto policy_builder =
      DeviceWeeklyScheduledSuspendTestPolicyBuilder().AddWeeklySuspendInterval(
          DayOfWeek::SATURDAY, base::Hours(0), DayOfWeek::SATURDAY,
          base::Hours(9));
  auto intervals = policy_builder.GetAsWeeklyTimeIntervals();

  auto* clock = helper.task_runner()->GetMockClock();
  auto duration =
      GetDuration(clock->Now(), intervals[0]->start()) - base::Minutes(5);
  helper.task_runner()->FastForwardBy(duration);

  SetPrefInLocalState(policy_builder.GetAsPrefValue());

  EXPECT_EQ(power_client->num_request_suspend_calls(), 0);
  EXPECT_EQ(power_manager_.user_activity_calls(), 0);

  helper.FastForwardTimeTo(intervals[0]->start());
  EXPECT_EQ(power_client->num_request_suspend_calls(), 1);

  // Resume the device before the end of the interval.
  power_client->SendSuspendDone();

  // Confirm that the device calls request suspend again in a week.
  helper.task_runner()->FastForwardBy(base::Days(7));
  EXPECT_EQ(power_client->num_request_suspend_calls(), 2);
}

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceWeeklyScheduledSuspendControllerTest,
    testing::ValuesIn(KioskMixin::ConfigsToAutoLaunchEachAppType()),
    KioskMixin::ConfigName);

}  // namespace ash
