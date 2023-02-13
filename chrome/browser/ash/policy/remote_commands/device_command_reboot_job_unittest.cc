// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/remote_commands/device_command_reboot_job.h"

#include <memory>
#include <utility>

#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/login/session/session_termination_manager.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

namespace {

constexpr RemoteCommandJob::UniqueIDType kUniqueID = 123456789;

constexpr base::TimeDelta kCommandAge = base::Minutes(5);
constexpr base::TimeDelta kAlmostExpiredCommandAge = base::Minutes(10);

constexpr base::TimeDelta kUserSessionRebootDelay = base::Minutes(5);

class ScopedLoginState {
 public:
  static ScopedLoginState CreateKiosk() {
    return ScopedLoginState(ash::LoginState::LOGGED_IN_NONE,
                            ash::LoginState::LOGGED_IN_USER_KIOSK);
  }

  static ScopedLoginState CreateLoggedOut() {
    return ScopedLoginState(ash::LoginState::LOGGED_IN_NONE,
                            ash::LoginState::LOGGED_IN_USER_NONE);
  }

  static ScopedLoginState CreateRegularUser() {
    return ScopedLoginState(ash::LoginState::LOGGED_IN_ACTIVE,
                            ash::LoginState::LOGGED_IN_USER_REGULAR);
  }

  ScopedLoginState(const ScopedLoginState& other) = delete;
  ScopedLoginState& operator=(const ScopedLoginState& other) = delete;

  ~ScopedLoginState() {
    ash::LoginState::Get()->SetLoggedInState(
        ash::LoginState::LOGGED_IN_NONE, ash::LoginState::LOGGED_IN_USER_NONE);

    ash::LoginState::Shutdown();
  }

 private:
  ScopedLoginState(ash::LoginState::LoggedInState state,
                   ash::LoginState::LoggedInUserType type) {
    DCHECK(!ash::LoginState::IsInitialized());

    ash::LoginState::Initialize();
    ash::LoginState::Get()->set_always_logged_in(false);
    ash::LoginState::Get()->SetLoggedInState(state, type);
  }
};

}  // namespace

class DeviceCommandRebootJobForTesting : public DeviceCommandRebootJob {
 public:
  template <class... Args>
  explicit DeviceCommandRebootJobForTesting(Args... args)
      : DeviceCommandRebootJob(std::forward<Args>(args)...) {}
};

class DeviceCommandRebootJobTest : public testing::Test {
 public:
  DeviceCommandRebootJobTest(const DeviceCommandRebootJobTest&) = delete;
  DeviceCommandRebootJobTest& operator=(const DeviceCommandRebootJobTest&) =
      delete;

 protected:
  DeviceCommandRebootJobTest() {
    chromeos::PowerManagerClient::InitializeFake();
    ash::CryptohomeMiscClient::InitializeFake();
    ash::SessionManagerClient::InitializeFake();
  }

  ~DeviceCommandRebootJobTest() override {
    ash::SessionManagerClient::Shutdown();
    ash::CryptohomeMiscClient::Shutdown();
    chromeos::PowerManagerClient::Shutdown();
  }

  base::Time Now() const { return task_environment_.GetMockClock()->Now(); }

  base::TimeTicks NowTicks() const {
    return task_environment_.GetMockTickClock()->NowTicks();
  }

  std::unique_ptr<DeviceCommandRebootJob> CreateAndInitializeCommand(
      base::TimeDelta age_of_command) {
    em::RemoteCommand command_proto;
    command_proto.set_type(em::RemoteCommand_Type_DEVICE_REBOOT);
    command_proto.set_command_id(kUniqueID);
    command_proto.set_age_of_command(age_of_command.InMilliseconds());

    auto job = std::make_unique<DeviceCommandRebootJobForTesting>(
        chromeos::FakePowerManagerClient::Get(), ash::LoginState::Get(),
        &session_termination_manager_,
        /*get_boot_time_callback=*/base::BindLambdaForTesting([this]() {
          return start_ticks_;
        }));

    EXPECT_TRUE(job->Init(NowTicks(), command_proto, em::SignedData()));

    return job;
  }

  std::unique_ptr<DeviceCommandRebootJob> CreateAndInitializeCommand() {
    return CreateAndInitializeCommand(base::TimeDelta());
  }

  base::test::SingleThreadTaskEnvironment task_environment_{
      base::test::SingleThreadTaskEnvironment::TimeSource::MOCK_TIME};
  const base::TimeTicks start_ticks_{NowTicks()};

  ash::SessionTerminationManager session_termination_manager_;
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
}

}  // namespace policy
