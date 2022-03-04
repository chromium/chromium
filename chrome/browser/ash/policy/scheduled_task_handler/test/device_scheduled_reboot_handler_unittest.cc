// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"

#include <memory>

#include "ash/components/settings/cros_settings_names.h"
#include "ash/constants/ash_features.h"
#include "base/feature_list.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/mock_user_manager.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_reboot_notifications_scheduler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "components/session_manager/core/session_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {
constexpr char kRebootTaskTimeFieldName[] = "reboot_time";
constexpr base::TimeDelta kExternalRebootDelay = base::Seconds(100);
}

class DeviceScheduledRebootHandlerForTest
    : public DeviceScheduledRebootHandler {
 public:
  DeviceScheduledRebootHandlerForTest(
      ash::CrosSettings* cros_settings,
      std::unique_ptr<ScheduledTaskExecutor> task_executor,
      std::unique_ptr<RebootNotificationsScheduler> notifications_scheduler)
      : DeviceScheduledRebootHandler(cros_settings,
                                     std::move(task_executor),
                                     std::move(notifications_scheduler)) {}

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
    auto notifications_scheduler =
        std::make_unique<FakeRebootNotificationsScheduler>(
            task_environment_.GetMockClock(),
            task_environment_.GetMockTickClock());
    notifications_scheduler_ = notifications_scheduler.get();
    device_scheduled_reboot_handler_ =
        std::make_unique<DeviceScheduledRebootHandlerForTest>(
            ash::CrosSettings::Get(), std::move(task_executor),
            std::move(notifications_scheduler));
    // Set 0 delay for tests.
    device_scheduled_reboot_handler_->SetRebootDelayForTest(base::TimeDelta());
    session_manager_.SetSessionState(session_manager::SessionState::ACTIVE);
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

  bool CheckNotificationStats(int notifications_shown, int dialogs_shown) {
    if (notifications_scheduler_->GetShowNotificationCalls() !=
        notifications_shown) {
      LOG(ERROR) << "Current notifications shown count: "
                 << notifications_scheduler_->GetShowNotificationCalls()
                 << " Expected notifications shown count: "
                 << notifications_shown;
      return false;
    }

    if (notifications_scheduler_->GetShowDialogCalls() != dialogs_shown) {
      LOG(ERROR) << "Current dialogs shown count: "
                 << notifications_scheduler_->GetShowDialogCalls()
                 << " Expected dialogs shown count: " << dialogs_shown;
      return false;
    }

    return true;
  }

  const base::TimeDelta GetRebootDelay() const {
    return (scheduled_task_executor_->GetScheduledTaskTime() -
            task_environment_.GetMockClock()->Now());
  }

  base::test::TaskEnvironment task_environment_;
  ash::MockUserManager* mock_user_manager_;  // Not owned.
  user_manager::ScopedUserManager user_manager_enabler_;
  FakeScheduledTaskExecutor* scheduled_task_executor_;
  std::unique_ptr<DeviceScheduledRebootHandlerForTest>
      device_scheduled_reboot_handler_;
  ash::ScopedTestingCrosSettings cros_settings_;
  device::TestWakeLockProvider wake_lock_provider_;
  FakeRebootNotificationsScheduler* notifications_scheduler_;
  base::test::ScopedFeatureList scoped_feature_list_;
  session_manager::SessionManager session_manager_;
};

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForKiosk) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
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
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
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

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfWeeklyUpdateCheckIsScheduledForKiosk) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
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

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfMonthlyRebootIsScheduledForKiosk) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
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

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledWithExternalDelay) {
  device_scheduled_reboot_handler_->SetRebootDelayForTest(kExternalRebootDelay);

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

  // Verify that final delay is equal to delay_from_now + external_delay.
  base::TimeDelta final_delay = GetRebootDelay();
  EXPECT_EQ(final_delay, delay_from_now + kExternalRebootDelay);
  task_environment_.FastForwardBy(final_delay - small_delay);
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
}

TEST_F(DeviceScheduledRebootHandlerTest,
       CheckIfDailyRebootIsScheduledForLockScreen) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {},
      /* disabled_features */ {ash::features::kDeviceForceScheduledReboot});
  session_manager_.SetSessionState(session_manager::SessionState::LOCKED);

  // Set device uptime to 10 minutes and schedule reboot in 30 minutes. Apply
  // grace time - reboot should not occur.
  notifications_scheduler_->SetUptime(base::Minutes(10));
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then verify reboot timer has not yet expired.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expired and the reboot is not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day at the same time and verify reboot is
  // executed.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // After the reboot, the current handler is destroyed and the new one is
  // created which will schedule reboot for the next day. Check that current
  // handler is not scheduling any more reboots.
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest, EnableForceRebootFeatureInKiosk) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {ash::features::kDeviceForceScheduledReboot},
      /* disabled_features */ {});

  // Set device uptime to 10 minutes and enable kiosk mode. We don't apply grace
  // period to kiosks, so reboot should occur.
  notifications_scheduler_->SetUptime(base::Minutes(10));
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillRepeatedly(testing::Return(true));

  // Calculate time 30 minutes from now and set the reboot policy to
  // happen daily at that time.
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and and then verify reboot timer has not yet expired.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expired and the reboot is executed.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest,
       EnableForceRebootFeatureNonKioskSession) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {ash::features::kDeviceForceScheduledReboot},
      /* disabled_features */ {});
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillRepeatedly(testing::Return(false));

  // Set device uptime to 10 minutes and schedule reboot in 30 minutes. Apply
  // grace time - reboot should not occur.
  notifications_scheduler_->SetUptime(base::Minutes(10));
  base::TimeDelta delay_from_now = base::Minutes(30);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to right before the
  // expected reboot and then check if the reboot timer has not yet expired.
  // Verify notifications are not shown.
  const base::TimeDelta small_delay = base::Milliseconds(1);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  int expected_notification_count = 0;
  int expected_dialog_count = 0;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));

  // Fast forward to the expected reboot time and then check if the
  // reboot timer has expred but the reboot is not executed.
  expected_scheduled_reboots += 1;
  task_environment_.FastForwardBy(small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));

  // Fast forward to the next day at the same time and verify reboot is executed
  // and notifications are shown.
  expected_scheduled_reboots += 1;
  expected_reboot_requests += 1;
  expected_notification_count += 1;
  expected_dialog_count += 1;
  task_environment_.FastForwardBy(base::Days(1));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

TEST_F(DeviceScheduledRebootHandlerTest, SimulateNotificationButtonClick) {
  scoped_feature_list_.InitWithFeatures(
      /* enabled_features */ {ash::features::kDeviceForceScheduledReboot},
      /* disabled_features */ {});
  EXPECT_CALL(*mock_user_manager_, IsLoggedInAsKioskApp())
      .WillRepeatedly(testing::Return(false));

  /// Schedule reboot to happen in 3 hours.
  base::TimeDelta delay_from_now = base::Hours(3);
  auto policy_and_next_reboot_time = scheduled_task_test_util::CreatePolicy(
      scheduled_task_executor_->GetTimeZone(),
      scheduled_task_executor_->GetCurrentTime(), delay_from_now,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  // Set a new scheduled reboot, fast forward to 5 minutes before the
  // expected reboot and then check that notification are shown, but the reboot
  // timer has not yet expired.
  const base::TimeDelta small_delay = base::Minutes(5);
  cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot,
      std::move(policy_and_next_reboot_time.first));
  int expected_scheduled_reboots = 0;
  int expected_reboot_requests = 0;
  int expected_notification_count = 1;
  int expected_dialog_count = 1;
  task_environment_.FastForwardBy(delay_from_now - small_delay);
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
  EXPECT_TRUE(CheckNotificationStats(expected_notification_count,
                                     expected_dialog_count));

  // Simulate reboot button click on the notification. This should execute the
  // reboot.
  notifications_scheduler_->SimulateRebootButtonClick();
  expected_reboot_requests += 1;
  EXPECT_TRUE(CheckStats(expected_scheduled_reboots, expected_reboot_requests));
}

}  // namespace policy
