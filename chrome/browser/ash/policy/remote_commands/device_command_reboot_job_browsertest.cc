// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_base_test.h"
#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_test_helpers.h"
#include "chrome/browser/ash/login/app_mode/test/web_kiosk_base_test.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/test/browser_test.h"

namespace em = enterprise_management;

namespace policy {

namespace {
template <typename BaseBrowserTest>
class DeviceCommandRebootBaseTest : public BaseBrowserTest {
  static_assert(
      std::is_base_of_v<MixinBasedInProcessBrowserTest, BaseBrowserTest>,
      "Must be MixinBasedInProcessBrowserTest");

 protected:
  em::RemoteCommandResult SendRemoteCommand(
      const enterprise_management::RemoteCommand& command) {
    return remote_commands_service_mixin_.SendRemoteCommand(command);
  }

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }

  DevicePolicyBuilder* device_policy() {
    return policy_helper_.device_policy();
  }

 private:
  DevicePolicyCrosTestHelper policy_helper_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_{&this->mixin_host_};
  RemoteCommandsServiceMixin remote_commands_service_mixin_{
      this->mixin_host_, policy_test_server_};
};
}  // namespace

class DeviceCommandRebootJobAutoLaunchKioskBrowserTest
    : public DeviceCommandRebootBaseTest<ash::LoginManagerTest> {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandRebootBaseTest::SetUpInProcessBrowserTestFixture();

    // Set up kiosk auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    ash::KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    policy_helper()->RefreshDevicePolicy();
  }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobAutoLaunchKioskBrowserTest,
                       RebootsInstantly) {
  ASSERT_TRUE(ash::LoginState::Get()->IsKioskSession());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommandResult result = SendRemoteCommand(
      RemoteCommandBuilder().SetType(em::RemoteCommand::DEVICE_REBOOT).Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

class DeviceCommandRebootJobKioskBrowserTest
    : public DeviceCommandRebootBaseTest<ash::KioskBaseTest> {
 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobKioskBrowserTest,
                       RebootsInstantly) {
  StartAppLaunchFromLoginScreen(NetworkStatus::kOnline);
  WaitForAppLaunchWithOptions(false /* check launch data */,
                              false /* terminate app */,
                              true /* keep app open */);

  ASSERT_TRUE(ash::LoginState::Get()->IsKioskSession());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommandResult result = SendRemoteCommand(
      RemoteCommandBuilder().SetType(em::RemoteCommand::DEVICE_REBOOT).Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

class DeviceCommandRebootJobWebKioskBrowserTest
    : public DeviceCommandRebootBaseTest<ash::WebKioskBaseTest> {};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobWebKioskBrowserTest,
                       RebootsInstantly) {
  InitializeRegularOnlineKiosk();

  ASSERT_TRUE(ash::LoginState::Get()->IsKioskSession());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommandResult result = SendRemoteCommand(
      RemoteCommandBuilder().SetType(em::RemoteCommand::DEVICE_REBOOT).Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

class DeviceCommandRebootJobAutoLaunchManagedGuestSessionBrowserTest
    : public DeviceCommandRebootBaseTest<ash::LoginManagerTest> {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandRebootBaseTest::SetUpInProcessBrowserTestFixture();

    // Set up MGS auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    ash::AppendAutoLaunchManagedGuestSessionAccount(&proto);

    policy_helper()->RefreshDevicePolicy();
  }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

IN_PROC_BROWSER_TEST_F(
    DeviceCommandRebootJobAutoLaunchManagedGuestSessionBrowserTest,
    RebootsManagedGuestSessionInstantlyWithZeroDelay) {
  ash::SessionStateWaiter(session_manager::SessionState::ACTIVE).Wait();

  ASSERT_TRUE(ash::LoginState::Get()->IsManagedGuestSessionUser());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommandResult result =
      SendRemoteCommand(RemoteCommandBuilder()
                            .SetType(em::RemoteCommand::DEVICE_REBOOT)
                            .SetPayload(R"({"user_session_delay_seconds": 0})")
                            .Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

class DeviceCommandRebootJobUserBrowserTest
    : public DeviceCommandRebootBaseTest<MixinBasedInProcessBrowserTest> {
 protected:
  void LoginManagedUser() { login_manager_mixin_.LoginAsNewRegularUser(); }

 private:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobUserBrowserTest,
                       RebootsLoginScreenInstantly) {
  ASSERT_FALSE(ash::LoginState::Get()->IsUserLoggedIn());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommandResult result = SendRemoteCommand(
      RemoteCommandBuilder().SetType(em::RemoteCommand::DEVICE_REBOOT).Build());

  EXPECT_EQ(result.result(), em::RemoteCommandResult::RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// TODO(b/225913691) Add user session test case.

}  // namespace policy
