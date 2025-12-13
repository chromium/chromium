// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <type_traits>

#include "chrome/browser/ash/app_mode/test/kiosk_mixin.h"
#include "chrome/browser/ash/app_mode/test/kiosk_test_utils.h"
#include "chrome/browser/ash/login/app_mode/test/managed_guest_session_test_helpers.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_test_helper.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/ash/policy/test_support/remote_commands_service_mixin.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/policy/device_policy/device_policy_builder.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "components/policy/core/common/remote_commands/test_support/remote_command_builders.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "components/session_manager/session_manager_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace em = enterprise_management;

namespace policy {

using ash::kiosk::test::LaunchAppManually;
using ash::kiosk::test::TheKioskApp;
using ash::kiosk::test::WaitKioskLaunched;

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

class DeviceCommandRebootJobKioskBrowserTest
    : public DeviceCommandRebootBaseTest<MixinBasedInProcessBrowserTest>,
      public testing::WithParamInterface<ash::KioskMixin::Config> {
 public:
  DeviceCommandRebootJobKioskBrowserTest() {
    // Force allow Chrome Apps in Kiosk, since they are default disabled since
    // M138.
    scoped_feature_list_.InitFromCommandLine("AllowChromeAppsInKioskSessions",
                                             "");
  }

  void SetUpOnMainThread() override {
    DeviceCommandRebootBaseTest<
        MixinBasedInProcessBrowserTest>::SetUpOnMainThread();
    if (IsManualLaunch()) {
      ASSERT_TRUE(LaunchAppManually(TheKioskApp()));
    }
    ASSERT_TRUE(WaitKioskLaunched());
  }

 private:
  bool IsManualLaunch() {
    return !GetParam().auto_launch_account_id.has_value();
  }

  ash::KioskMixin kiosk_{&mixin_host_,
                         /*cached_configuration=*/GetParam()};
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_P(DeviceCommandRebootJobKioskBrowserTest,
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

INSTANTIATE_TEST_SUITE_P(
    All,
    DeviceCommandRebootJobKioskBrowserTest,
    // TODO(crbug.com/379633748): Add IWA.
    ::testing::Values(
        ash::KioskMixin::Config{
            /*name=*/"WebAppAutoLaunch",
            ash::KioskMixin::AutoLaunchAccount{
                ash::KioskMixin::SimpleWebAppOption().account_id},
            {ash::KioskMixin::SimpleWebAppOption()}},
        ash::KioskMixin::Config{
            /*name=*/"ChromeAppAutoLaunch",
            ash::KioskMixin::AutoLaunchAccount{
                ash::KioskMixin::SimpleChromeAppOption().account_id},
            {ash::KioskMixin::SimpleChromeAppOption()}},
        ash::KioskMixin::Config{/*name=*/"WebAppManualLaunch",
                                /*auto_launch_account_id=*/{},
                                {ash::KioskMixin::SimpleWebAppOption()}},
        ash::KioskMixin::Config{/*name=*/"ChromeAppManualLaunch",
                                /*auto_launch_account_id=*/{},
                                {ash::KioskMixin::SimpleChromeAppOption()}}),
    ash::KioskMixin::ConfigName);

class DeviceCommandRebootJobAutoLaunchManagedGuestSessionBrowserTest
    : public DeviceCommandRebootBaseTest<ash::LoginManagerTest> {
 protected:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceCommandRebootBaseTest::SetUpInProcessBrowserTestFixture();

    // Set up MGS auto-launch mode.
    em::ChromeDeviceSettingsProto& proto(device_policy()->payload());
    ash::test::AppendAutoLaunchManagedGuestSessionAccount(&proto);

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
