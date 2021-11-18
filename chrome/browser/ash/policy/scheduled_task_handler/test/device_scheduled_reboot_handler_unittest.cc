// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"

#include <memory>

#include "ash/components/settings/cros_settings_names.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kRebootTaskTimeFieldName[] = "reboot_time";
}

class DeviceScheduledRebootHandlerForTest
    : public DeviceScheduledRebootHandler {
 public:
  DeviceScheduledRebootHandlerForTest(
      ash::CrosSettings* cros_settings,
      std::unique_ptr<ScheduledTaskExecutor> task_executor)
      : DeviceScheduledRebootHandler(cros_settings, std::move(task_executor)) {}

  DeviceScheduledRebootHandlerForTest(
      const DeviceScheduledRebootHandlerForTest&) = delete;
  DeviceScheduledRebootHandlerForTest& operator=(
      const DeviceScheduledRebootHandlerForTest&) = delete;

  ~DeviceScheduledRebootHandlerForTest() override {
    TestingBrowserProcess::GetGlobal()->ShutdownBrowserPolicyConnector();
  }

  int GetRebootTimerExpirations() const { return reboot_timer_expirations_; }

 private:
  void OnRebootTimerExpired() override {
    ++reboot_timer_expirations_;
    DeviceScheduledRebootHandler::OnRebootTimerExpired();
  }

  // Number of calls to |OnRebootTimerExpired|.
  int reboot_timer_expirations_ = 0;
};

class DeviceScheduledRebootHandlerTest : public testing::Test {
 public:
  DeviceScheduledRebootHandlerTest()
      : task_environment_(base::test::TaskEnvironment::MainThreadType::IO,
                          base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        mock_user_manager_(new ash::MockUserManager),
        user_manager_enabler_(base::WrapUnique(mock_user_manager_)) {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&wake_lock_provider_)));
    chromeos::PowerManagerClient::InitializeFake();
    chromeos::FakePowerManagerClient::Get()->set_tick_clock(
        task_environment_.GetMockTickClock());

    auto task_executor = std::make_unique<FakeScheduledTaskExecutor>(
        task_environment_.GetMockClock());
    scheduled_task_executor_ = task_executor.get();
    device_scheduled_reboot_handler_ =
        std::make_unique<DeviceScheduledRebootHandlerForTest>(
            ash::CrosSettings::Get(), std::move(task_executor));
  }

  ~DeviceScheduledRebootHandlerTest() override {
    device_scheduled_reboot_handler_.reset();
    chromeos::PowerManagerClient::Shutdown();
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
  }

 protected:
  bool CheckStats(int expected_scheduled_reboots,
                  int expected_reboot_requests) {
    if (device_scheduled_reboot_handler_->GetRebootTimerExpirations() !=
        expected_scheduled_reboots) {
      LOG(ERROR)
          << "Current reboot timer expirations: "
          << device_scheduled_reboot_handler_->GetRebootTimerExpirations()
          << " Expected reboot timer expirations: "
          << expected_scheduled_reboots;
      return false;
    }

    if (chromeos::FakePowerManagerClient::Get()->num_request_restart_calls() !=
        expected_reboot_requests) {
      LOG(ERROR) << "Current reboot requests: "
                 << chromeos::FakePowerManagerClient::Get()
                        ->num_request_restart_calls()
                 << " Expected reboot requests: " << expected_reboot_requests;
      return false;
    }

    return true;
  }

  base::test::TaskEnvironment task_environment_;
  ash::MockUserManager* mock_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  FakeScheduledTaskExecutor* scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledRebootHandlerForTest>
      device_scheduled_reboot_handler_;
  ash::ScopedTestingCrosSettings cros_settings_;
  device::TestWakeLockProvider wake_lock_provider_;
};

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForKiosk) {
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillRepeatedly(testing::Return(true));

  // Calculate time from one hour from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if an reboot is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot is scheduled.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // After the reboot, the current handler is destroyed and the new one is
  // created which will schedule reboot for the next day. Check that current
  // handler is not scheduling any more reboots.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForNonKiosk) {
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillRepeatedly(testing::Return(false));

  // Calculate time from one hour from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if an reboot is not scheduled.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot is scheduled, but not executed since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day and then check if the reboot is scheduled
  // again.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Switch to the kiosk mode, fast forward to the next day and check that the
  // reboot is scheduled and executed.
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillOnce(testing::Return(true));
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest, CheckIfWeeklyUpdateCheckIsScheduled) {
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillOnce(testing::Return(false));
  // Set the first reboot to happen 49 hours from now (i.e. 1 hour from 2
  // days from now) and then weekly after.
  base::TimeDelta delay_from_now = base::Hours(49);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kWeekly, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot setting, fast forward to right before the
  // expected reboot and then check if a reboot is not scheduled.
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the reboot
  // is scheduled, but not executed, since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Switch to the kiosk mode, fast forward to the next week and check that the
  // reboot is scheduled and executed.
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillOnce(testing::Return(true));
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(7));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest, CheckIfMonthlyRebootIsScheduled) {
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillOnce(testing::Return(false));
  // Set the first reboot to happen 1 hour from now.
  base::TimeDelta delay_from_now = base::Hours(1);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kMonthly, kRebootTaskTimeFieldName);
  auto scheduled_reboot_data = scheduled_task_util::ParseScheduledTask(
      policy_and_next_reboot_time.first, kRebootTaskTimeFieldName);
  ASSERT_TRUE(scheduled_reboot_data);
  ASSERT_TRUE(scheduled_reboot_data->day_of_month);
  auto first_reboot_icu_time = std::move(policy_and_next_reboot_time.second);

  // Set a new scheduled reboot setting, fast forward to right before the
  // expected reboot and then check if a reboot is not scheduled.
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the reboot
  // is scheduled, but not executed since we are not in the kiosk mode.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // The next reboot should happen at the same day of month next month. Switch
  // to the kiosk mode and verify the reboot is executed.
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillOnce(testing::Return(true));
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  EXPECT_TRUE(scheduled_task_test_util::AdvanceTimeAndSetDayOfMonth(
      scheduled_reboot_data->day_of_month.value(),
      first_reboot_icu_time.get()));
  base::Time second_reboot_time =
      scheduled_task_test_util::IcuToBaseTime(*first_reboot_icu_time);
  absl::optional<base::TimeDelta> second_reboot_delay =
      second_reboot_time - scheduled_task_executor_->GetCurrentTime();
  ASSERT_TRUE(second_reboot_delay.has_value());
  task_environment_.FastForwardBy(second_reboot_delay.value());
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

}  // namespace policy
