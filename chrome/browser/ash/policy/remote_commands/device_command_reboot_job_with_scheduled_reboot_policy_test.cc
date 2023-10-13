// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"

#include <memory>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/functional/bind.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job_test_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/device_scheduled_reboot_handler.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/test/scheduled_task_test_util.h"
#include "chrome/browser/ash/settings/scoped_testing_cros_settings.h"
#include "chrome/browser/ash/settings/stub_cros_settings_provider.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/fake_user_manager.h"
#include "components/user_manager/scoped_user_manager.h"
#include "services/device/public/cpp/test/test_wake_lock_provider.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr base::TimeDelta kUserSessionRebootDelay = base::Minutes(5);

constexpr base::TimeDelta kScheduledRebootNotificationDelay = base::Hours(1);
constexpr base::TimeDelta kScheduledRebootDialogDelay = base::Minutes(5);

constexpr char kRebootTaskTimeFieldName[] = "reboot_time";

std::unique_ptr<user_manager::FakeUserManager>
CreateFakeUserManagerForRegularSession() {
  auto manager = std::make_unique<user_manager::FakeUserManager>();

  const AccountId id = AccountId::FromUserEmail("user@user.net");
  manager->AddUser(id);
  manager->UserLoggedIn(id, /*username_hash=*/id.GetUserEmail(),
                        /*browser_restart=*/false, /*is_child=*/false);

  return manager;
}

class ScopedTestWakeLockProvider {
 public:
  ScopedTestWakeLockProvider() {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::BindRepeating(&device::TestWakeLockProvider::BindReceiver,
                            base::Unretained(&test_wake_lock_provider_)));
  }

  ~ScopedTestWakeLockProvider() {
    ScopedWakeLock::OverrideWakeLockProviderBinderForTesting(
        base::NullCallback());
  }

 private:
  device::TestWakeLockProvider test_wake_lock_provider_;
};

}  // namespace

// Tests interaction between the reboot command and DeviceScheduledReboot
// policy.
class DeviceCommandRebootJobWithScheduledRebootPolicyTest
    : public DeviceCommandRebootJobTestBase,
      public testing::Test {
 public:
  DeviceCommandRebootJobWithScheduledRebootPolicyTest(
      const DeviceCommandRebootJobWithScheduledRebootPolicyTest&) = delete;
  DeviceCommandRebootJobWithScheduledRebootPolicyTest& operator=(
      const DeviceCommandRebootJobWithScheduledRebootPolicyTest&) = delete;

 protected:
  DeviceCommandRebootJobWithScheduledRebootPolicyTest() {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kScheduledRebootGracePeriodInSecondsForTesting,
        base::NumberToString(0));
  }

  ~DeviceCommandRebootJobWithScheduledRebootPolicyTest() override = default;

  std::unique_ptr<DeviceScheduledRebootHandler> CreateScheduledRebootHandler(
      std::unique_ptr<FakeScheduledTaskExecutor> fake_task_executor) {
    auto device_scheduled_reboot_handler =
        std::make_unique<DeviceScheduledRebootHandler>(
            ash::CrosSettings::Get(), std::move(fake_task_executor),
            fake_notifications_scheduler_.get());
    device_scheduled_reboot_handler->SetRebootDelayForTest(base::TimeDelta());
    return device_scheduled_reboot_handler;
  }

  std::unique_ptr<DeviceCommandRebootJob> CreateAndInitializeCommand() {
    return DeviceCommandRebootJobTestBase::CreateAndInitializeCommand(
        /*age_of_command=*/base::TimeDelta(), kUserSessionRebootDelay);
  }

  ash::ScopedTestingCrosSettings scoped_cros_settings_;
  base::test::ScopedCommandLine scoped_command_line_;

 private:
  user_manager::ScopedUserManager scoped_user_manager_{
      CreateFakeUserManagerForRegularSession()};
  ScopedLoginState scoped_login_state_{ScopedLoginState::CreateRegularUser()};
  ScopedTestWakeLockProvider scoped_test_wake_lock_provider_;
};

// Tests that the reboot command issued before the kDeviceScheduledReboot policy
// timestamp, triggers reboot instead of the policy.
TEST_F(DeviceCommandRebootJobWithScheduledRebootPolicyTest,
       RebootsWhenScheduledRebootPolicyIsAfterTimeoutInSession) {
  // A     B      C         D         E
  // 0min  1h     1h54min   1h59min   2h
  // --------------------------------------
  // ^     ^      ^         ^         ^
  // |     |      |         |         |
  // boot  |   command      |        scheduled reboot
  //     notification     reboot

  // A. Boot time and setup.
  auto fake_task_executor = std::make_unique<FakeScheduledTaskExecutor>(
      task_environment_.GetMockClock());

  // Schedule reboot policy to reboot in 2 hours.
  const base::TimeDelta delay_till_policy_reboot = base::Hours(2);
  auto [policy_value, reboot_time] = scheduled_task_test_util::CreatePolicy(
      fake_task_executor->GetTimeZone(), Now(), delay_till_policy_reboot,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  std::unique_ptr<DeviceScheduledRebootHandler>
      device_scheduled_reboot_handler =
          CreateScheduledRebootHandler(std::move(fake_task_executor));

  scoped_cros_settings_.device_settings()->Set(ash::kDeviceScheduledReboot,
                                               std::move(policy_value));

  // Check that nothing happened yet.
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 0);

  // B. Fastforward to the point when the policy shows reboot notification.
  task_environment_.FastForwardBy(delay_till_policy_reboot -
                                  kScheduledRebootNotificationDelay);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // C. Fastforward closer to the scheduled reboot. Leave some time for the
  // command.
  task_environment_.FastForwardBy(kScheduledRebootNotificationDelay -
                                  kUserSessionRebootDelay - base::Minutes(1));

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that reboot notification is shown after timer starts. Fast forward
  // without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);

  // D. Fastforward till the user session timeout expires.
  task_environment_.FastForwardBy(kUserSessionRebootDelay);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 3);
}

// Tests that the reboot command issued before scheduled reboot resets both
// the reboot notification and dialog.
TEST_F(DeviceCommandRebootJobWithScheduledRebootPolicyTest,
       TakesOverNotificationAndDialog) {
  // A     B      C         D         E
  // 0min  1h     1h55min   1h55min   1h59min
  // -----------------------------------------
  // ^     ^      ^         ^         ^
  // |     |      |         |         |
  // boot  |   dialog       |        command reboot
  //     notification      command

  // Use smaller user session delay for reboot command so it's triggered
  // before the scheduled reboot.
  constexpr base::TimeDelta kSmallUserSessionDelay = base::Minutes(4);
  scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
      ash::switches::kRemoteRebootCommandDelayInSecondsForTesting,
      base::NumberToString(kSmallUserSessionDelay.InSeconds()));

  // A. Boot time and setup.
  auto fake_task_executor = std::make_unique<FakeScheduledTaskExecutor>(
      task_environment_.GetMockClock());

  // Schedule reboot policy to reboot in 2 hours.
  const base::TimeDelta delay_till_policy_reboot = base::Hours(2);
  auto [policy_value, reboot_time] = scheduled_task_test_util::CreatePolicy(
      fake_task_executor->GetTimeZone(), Now(), delay_till_policy_reboot,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  std::unique_ptr<DeviceScheduledRebootHandler>
      device_scheduled_reboot_handler =
          CreateScheduledRebootHandler(std::move(fake_task_executor));

  scoped_cros_settings_.device_settings()->Set(ash::kDeviceScheduledReboot,
                                               std::move(policy_value));

  // Check that nothing happened yet.
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 0);

  // C. Fastforward to the point when the policy shows reboot dialog.
  task_environment_.FastForwardBy(delay_till_policy_reboot -
                                  kScheduledRebootDialogDelay);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // D. Issue the command. Check it takes over the notification and the dialog.
  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that reboot notification is shown after timer starts. Fast forward
  // without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);

  // E. Fastforward till the user session timeout expires.
  task_environment_.FastForwardBy(kSmallUserSessionDelay);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 3);
}

// Tests that the command issued close to the DeviceScheduledReboot policy
// timestamp does not trigger notifications and reboot, but the policy does.
TEST_F(DeviceCommandRebootJobWithScheduledRebootPolicyTest,
       RebootsWhenScheduledRebootPolicyIsBeforeTimeoutInSession) {
  // A     B         C         D      E
  // 0min  1h        1h56min   2h     2h1min
  // ---------------------------------------
  // ^     ^         ^         ^      ^
  // |     |         |         |      |
  // boot  |      command      |     command reboot
  //     notification       scheduled reboot

  // A. Boot time and setup.
  auto fake_task_executor = std::make_unique<FakeScheduledTaskExecutor>(
      task_environment_.GetMockClock());

  // Schedule reboot policy to reboot in 2 hours.
  const base::TimeDelta delay_till_policy_reboot = base::Hours(2);
  auto [policy_value, reboot_time] = scheduled_task_test_util::CreatePolicy(
      fake_task_executor->GetTimeZone(), Now(), delay_till_policy_reboot,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);

  std::unique_ptr<DeviceScheduledRebootHandler>
      device_scheduled_reboot_handler =
          CreateScheduledRebootHandler(std::move(fake_task_executor));

  scoped_cros_settings_.device_settings()->Set(ash::kDeviceScheduledReboot,
                                               std::move(policy_value));

  // Check that nothing happened yet.
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 0);

  // B. Fastforward to the point when the policy shows reboot notification.
  task_environment_.FastForwardBy(delay_till_policy_reboot -
                                  kScheduledRebootNotificationDelay);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 0);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // C. Fastforward until reboot dialog is shown and a bit more so that the
  // command delay expires after the policy.
  task_environment_.FastForwardBy(kScheduledRebootNotificationDelay -
                                  kUserSessionRebootDelay + base::Minutes(1));

  // Check that the policy triggered the notification and the dialog.
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that the command did not update notifications. Fast forward
  // without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // D. Fastforward until the policy triggers reboot.
  task_environment_.FastForwardBy(kUserSessionRebootDelay - base::Minutes(1));
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

// Tests that the DeviceScheduledReboot policy triggers the reboot when
// scheduled while the command is in progress
TEST_F(DeviceCommandRebootJobWithScheduledRebootPolicyTest,
       RebootsWhenScheduledRebootPolicyIsRescheduledBeforeTimeoutInSession) {
  // A      B        C                D
  // 0min   2h       2h3min           2h5min
  // ---------------------------------------
  // ^      ^        ^                ^
  // |      |        |                |
  // boot   command  |                command reboot
  //                 scheduled reboot

  // A. Boot time and setup.
  task_environment_.FastForwardBy(base::Hours(2));

  // B. Issue the reboot command. In session timeout starts.
  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that the command showed notification. Fast forward without an actual
  // time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Schedule reboot via policy within delay.
  auto fake_task_executor = std::make_unique<FakeScheduledTaskExecutor>(
      task_environment_.GetMockClock());
  const base::TimeDelta delay_till_policy_reboot =
      kUserSessionRebootDelay - base::Minutes(1);
  auto [policy_value, reboot_time] = scheduled_task_test_util::CreatePolicy(
      fake_task_executor->GetTimeZone(), Now(), delay_till_policy_reboot,
      ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);
  std::unique_ptr<DeviceScheduledRebootHandler>
      device_scheduled_reboot_handler =
          CreateScheduledRebootHandler(std::move(fake_task_executor));
  scoped_cros_settings_.device_settings()->Set(ash::kDeviceScheduledReboot,
                                               std::move(policy_value));

  // Check that the policy reset notification. Fast forward without an actual
  // time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);

  // C. Fastforward until the policy triggers reboot.
  task_environment_.FastForwardBy(delay_till_policy_reboot);
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

// Tests that the reboot command triggers reboot when ScheduledReboot policy
// scheduled before command timeout is being rescheduled after the timeout.
TEST_F(DeviceCommandRebootJobWithScheduledRebootPolicyTest,
       RebootsWhenScheduledRebootPolicyIsRescheduledAfterTimeoutInSession) {
  // A      B        C                D               E
  // 0min   2h       2h4min           2h5min          2h6min
  // ---------------------------------------------------------
  // ^      ^        ^                ^               ^
  // |      |        |                |               |
  // boot   command  |                command reboot  |
  //                 first scheduled reboot           second scheduled reboot

  // A. Boot time and setup.
  task_environment_.FastForwardBy(base::Hours(2));

  // B. Issue the reboot command. In session timeout starts.
  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that the command showed notification. Fast forward without an actual
  // time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Schedule reboot via policy within delay.
  auto fake_task_executor = std::make_unique<FakeScheduledTaskExecutor>(
      task_environment_.GetMockClock());
  auto* fake_task_executor_ptr = fake_task_executor.get();
  const base::TimeDelta delay_till_policy_reboot_before_command =
      kUserSessionRebootDelay - base::Minutes(1);
  auto [policy_value_before_command, reboot_time_before_command] =
      scheduled_task_test_util::CreatePolicy(
          fake_task_executor_ptr->GetTimeZone(), Now(),
          delay_till_policy_reboot_before_command,
          ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);
  std::unique_ptr<DeviceScheduledRebootHandler>
      device_scheduled_reboot_handler =
          CreateScheduledRebootHandler(std::move(fake_task_executor));
  scoped_cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot, std::move(policy_value_before_command));

  // Check that the policy reset notification. Fast forward without an actual
  // time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 2);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);

  // Reschedule the policy after the command timeout.
  const base::TimeDelta delay_till_policy_reboot_after_command =
      kUserSessionRebootDelay + base::Minutes(1);
  auto [policy_value_after_command, reboot_time_after_command] =
      scheduled_task_test_util::CreatePolicy(
          fake_task_executor_ptr->GetTimeZone(), Now(),
          delay_till_policy_reboot_after_command,
          ScheduledTaskExecutor::Frequency::kDaily, kRebootTaskTimeFieldName);
  scoped_cros_settings_.device_settings()->Set(
      ash::kDeviceScheduledReboot, std::move(policy_value_after_command));

  // Check that the command shows notification again. Fast forward without an
  // actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 3);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 3);
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 3);

  // C. Fastforward until the initial scheduled reboot. Check that reboot does
  // not happen.
  task_environment_.FastForwardBy(delay_till_policy_reboot_before_command);
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  // D. Fastforward until the command timeout. Check that reboot happens.
  task_environment_.FastForwardBy(kUserSessionRebootDelay -
                                  delay_till_policy_reboot_before_command);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 4);
}

}  // namespace policy
