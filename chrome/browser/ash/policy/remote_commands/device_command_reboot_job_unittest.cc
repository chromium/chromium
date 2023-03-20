// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"

#include <memory>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job_test_util.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace policy {

namespace {

constexpr base::TimeDelta kCommandAge = base::Minutes(5);
constexpr base::TimeDelta kAlmostExpiredCommandAge = base::Minutes(10);

constexpr base::TimeDelta kUserSessionRebootDelay = base::Minutes(5);

}  // namespace

class DeviceCommandRebootJobTest : public DeviceCommandRebootJobTestBase,
                                   public testing::Test {
 protected:
  DeviceCommandRebootJobTest() {
    scoped_command_line_.GetProcessCommandLine()->AppendSwitchASCII(
        ash::switches::kRemoteRebootCommandTimeoutInSecondsForTesting,
        base::NumberToString(kUserSessionRebootDelay.InSeconds()));
  }

 private:
  base::test::ScopedCommandLine scoped_command_line_;
};

// Test that the command expires after default expiration time.
TEST_F(DeviceCommandRebootJobTest, ExpiresAfterExpirationTime) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  auto old_command = CreateAndInitializeCommand();
  task_environment_.FastForwardBy(kAlmostExpiredCommandAge + base::Seconds(1));

  EXPECT_FALSE(old_command->Run(Now(), NowTicks(),
                                RemoteCommandJob::FinishedCallback()));
  EXPECT_EQ(old_command->status(), RemoteCommandJob::EXPIRED);
}

// Test that the command does not expire before default expiration time.
TEST_F(DeviceCommandRebootJobTest, DoesNotExpireBeforeExpirationTime) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  auto fresh_command = CreateAndInitializeCommand();
  task_environment_.FastForwardBy(kAlmostExpiredCommandAge);

  EXPECT_TRUE(fresh_command->Run(Now(), NowTicks(),
                                 RemoteCommandJob::FinishedCallback()));
}

// Test that the command does not trigger reboot if boot happened after the
// command was issued
TEST_F(DeviceCommandRebootJobTest, DoesNotRebootIfBootedRecently) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  // Boot time is considered as 0 time ticks. Initialize the command so it's
  // issued before boot time but has not expired yet.
  // The implementation relies on base::TimeTicks supporting negative values.
  //   -5min        0min                 1min
  // ---------------------------------------
  //   ^            ^                    ^
  //   |            |                    |
  //   issue_time   boot_time            now

  // Create the command with `kCommandAge` age before boot time.
  auto command = CreateAndInitializeCommand(kCommandAge);

  base::test::TestFuture<void> future;
  EXPECT_TRUE(command->Run(Now(), NowTicks(), future.GetCallback()));

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);
}

// Test that the command instantly reboots the device in a kiosk mode.
TEST_F(DeviceCommandRebootJobTest, RebootsKioskInstantly) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// Test that the command instantly reboots the device outside of a user session
// on the login screen.
TEST_F(DeviceCommandRebootJobTest, RebootsInstantlyOutsideOfSession) {
  auto scoped_login_state = ScopedLoginState::CreateLoggedOut();

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// Tests that in the user session, the command does not instantly reboots util a
// user logs out.
TEST_F(DeviceCommandRebootJobTest, RebootsOnUserLogout) {
  auto scoped_login_state = ScopedLoginState::CreateRegularUser();

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Fast forward time a little but before command timeout expires. Check that
  // reboot has not happened yet.
  task_environment_.FastForwardBy(kUserSessionRebootDelay / 2);

  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  session_termination_manager_.StopSession(
      login_manager::SessionStopReason::USER_REQUESTS_SIGNOUT);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
}

// Tests that in the user session, the command shows the notification, waits for
// timeout and only then reboots.
TEST_F(DeviceCommandRebootJobTest,
       ShowsNotificationAndRebootsAfterTimeoutInSession) {
  auto scoped_login_state = ScopedLoginState::CreateRegularUser();

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that reboot notification is shown after timer starts. Fast forward
  // without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);

  // Fast forward time a little but before command timeout expires. Check that
  // reboot has not happened yet.
  task_environment_.FastForwardBy(kUserSessionRebootDelay / 2);

  // Before being shown for the first time, the notification scheduler closes
  // itself. That's why we expect 1 instead of 0.
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Check that the command is still running as timeout is not reached yet.
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  // Finish fastforwarding the timeout.
  task_environment_.FastForwardBy(kUserSessionRebootDelay / 2);
  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_TRUE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);
}

// Tests that in the user session, the command shows the notification and
// reboots when a user clicks the reboot button.
TEST_F(DeviceCommandRebootJobTest,
       ShowsNotificationAndRebootsWhenUserClicksRestartButtonInSession) {
  auto scoped_login_state = ScopedLoginState::CreateRegularUser();

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that reboot notification is shown after timer starts. Fast forward
  // without an actual time so that the timer task is triggered.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(fake_notifications_scheduler_->GetShowNotificationCalls(), 1);
  EXPECT_EQ(fake_notifications_scheduler_->GetShowDialogCalls(), 1);

  // Fast forward time a little but before command timeout expires. Check that
  // reboot has not happened yet.
  task_environment_.FastForwardBy(kUserSessionRebootDelay / 2);

  // Before being shown for the first time, the notification scheduler closes
  // itself. That's why we expect 1 instead of 0.
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 1);

  // Check that the command is still running as timeout is not reached yet.
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  // Simulate a user clicking the reboot button on dialog and check the reboot
  // is immediately requested.
  fake_notifications_scheduler_->SimulateRebootButtonClick();

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
  EXPECT_FALSE(prefs_->GetBoolean(ash::prefs::kShowPostRebootNotification));
  EXPECT_EQ(fake_notifications_scheduler_->GetCloseNotificationCalls(), 2);
}

// Tests that the command delays execution until power manager availability is
// known, and reboots once power manager is available.
TEST_F(DeviceCommandRebootJobTest, RebootsWhenPowerManagerIsAvailable) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/absl::nullopt);

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  // Check that command is still running while power manager availability is
  // unknown.
  task_environment_.FastForwardBy(base::TimeDelta());
  EXPECT_EQ(command->status(), RemoteCommandJob::RUNNING);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/true);

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::SUCCEEDED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// Tests that the command fails when power manager is not initialized.
TEST_F(DeviceCommandRebootJobTest,
       ReportsFailureWhenPowerManagerIsNotInitialized) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  chromeos::FakePowerManagerClient::Get()->Shutdown();
  EXPECT_FALSE(chromeos::PowerManagerClient::Get());

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::FAILED);
}

// Tests that the command fails when power manager is unavailable.
TEST_F(DeviceCommandRebootJobTest,
       ReportsFailureWhenPowerManagerIsUnavailable) {
  auto scoped_login_state = ScopedLoginState::CreateKiosk();

  chromeos::FakePowerManagerClient::Get()->SetServiceAvailability(
      /*availability=*/false);

  auto command = CreateAndInitializeCommand();
  base::test::TestFuture<void> future;
  command->Run(Now(), NowTicks(), future.GetCallback());

  ASSERT_TRUE(future.Wait());
  EXPECT_EQ(command->status(), RemoteCommandJob::FAILED);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);
}

}  // namespace policy
