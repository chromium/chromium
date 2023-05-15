// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/time/time.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/policy/core/browser_policy_connector_ash.h"
#include "chrome/browser/ash/policy/core/device_cloud_policy_manager_ash.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/policy/chrome_browser_policy_connector.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/remote_commands/remote_commands_service.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/policy/test_support/embedded_policy_test_server.h"
#include "components/policy/test_support/remote_commands_result_waiter.h"
#include "content/public/test/browser_test.h"

namespace em = enterprise_management;

namespace policy {
namespace {

constexpr int kFakePolicyPublicKeyVersion = 1;
constexpr int kUniqueCommandId = 123456789;

class DeviceCommandRebootJobBaseTest : public ash::LoginManagerTest {
 public:
  DeviceCommandRebootJobBaseTest(const DeviceCommandRebootJobBaseTest&) =
      delete;
  DeviceCommandRebootJobBaseTest& operator=(
      const DeviceCommandRebootJobBaseTest&) = delete;

 protected:
  DeviceCommandRebootJobBaseTest() = default;
  ~DeviceCommandRebootJobBaseTest() override = default;

  DevicePolicyCrosTestHelper* policy_helper() { return &policy_helper_; }

  DevicePolicyBuilder* device_policy() {
    return policy_helper()->device_policy();
  }

  void SetUpInProcessBrowserTestFixture() override {
    LoginManagerTest::SetUpInProcessBrowserTestFixture();

    device_policy()->policy_data().set_public_key_version(
        kFakePolicyPublicKeyVersion);
    device_policy()->Build();
    ash::FakeSessionManagerClient::Get()->set_device_policy(
        device_policy()->GetBlob());
  }

  void AddPendingRemoteCommand(const em::RemoteCommand& command) {
    policy_test_server_mixin_.server()
        ->remote_commands_state()
        ->AddPendingRemoteCommand(command);
  }

  void SendDeviceRemoteCommandsRequest() {
    g_browser_process->browser_policy_connector()
        ->ScheduleServiceInitialization(0);

    DeviceCloudPolicyManagerAsh* policy_manager =
        g_browser_process->platform_part()
            ->browser_policy_connector_ash()
            ->GetDeviceCloudPolicyManager();

    ASSERT_TRUE(policy_manager);
    ASSERT_TRUE(policy_manager->core()->client());

    RemoteCommandsService* remote_commands_service =
        policy_manager->core()->remote_commands_service();
    remote_commands_service->FetchRemoteCommands();
  }

  em::RemoteCommandResult WaitForResult(int command_id) {
    return RemoteCommandsResultWaiter(
               policy_test_server_mixin_.server()->remote_commands_state(),
               command_id)
        .WaitAndGetResult();
  }

 private:
  DevicePolicyCrosTestHelper policy_helper_;
  ash::EmbeddedPolicyTestServerMixin policy_test_server_mixin_{&mixin_host_};
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

}  // namespace

class DeviceCommandRebootJobKioskBrowserTest
    : public DeviceCommandRebootJobBaseTest {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandRebootJobBaseTest::SetUpInProcessBrowserTestFixture();

    // Set up kiosk auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    ash::KioskAppsMixin::AppendAutoLaunchKioskAccount(&proto);
    policy_helper()->RefreshDevicePolicy();
  }
};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobKioskBrowserTest,
                       RebootsKioskInstantly) {
  ASSERT_TRUE(ash::LoginState::Get()->IsKioskSession());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommand command;
  command.set_type(em::RemoteCommand_Type_DEVICE_REBOOT);
  command.set_command_id(kUniqueCommandId);
  command.set_age_of_command(base::TimeDelta().InMilliseconds());
  AddPendingRemoteCommand(command);

  SendDeviceRemoteCommandsRequest();

  em::RemoteCommandResult result = WaitForResult(kUniqueCommandId);
  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// TODO(b/225913691) Add test case for manually launched kiosk.

class DeviceCommandRebootJobUserBrowserTest
    : public DeviceCommandRebootJobBaseTest {
 protected:
  void LoginManagedUser() { login_manager_mixin_.LoginAsNewRegularUser(); }

 private:
  ash::LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(DeviceCommandRebootJobUserBrowserTest,
                       RebootsLoginScreenInstantly) {
  ASSERT_FALSE(ash::LoginState::Get()->IsUserLoggedIn());
  ASSERT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 0);

  em::RemoteCommand command;
  command.set_type(em::RemoteCommand_Type_DEVICE_REBOOT);
  command.set_command_id(kUniqueCommandId);
  command.set_age_of_command(base::TimeDelta().InMilliseconds());
  AddPendingRemoteCommand(command);

  SendDeviceRemoteCommandsRequest();

  em::RemoteCommandResult result = WaitForResult(kUniqueCommandId);
  EXPECT_EQ(result.result(), em::RemoteCommandResult_ResultType_RESULT_SUCCESS);
  EXPECT_EQ(
      chromeos::FakePowerManagerClient::Get()->num_request_restart_calls(), 1);
}

// TODO(b/225913691) Add user session test case.

}  // namespace policy
