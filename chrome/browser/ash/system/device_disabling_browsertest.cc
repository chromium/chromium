// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/system/device_disabling_manager.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_state_informer.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_service_client.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "dbus/object_path.h"

namespace ash {
namespace system {

namespace {

void ErrorCallbackFunction(const std::string& error_name,
                           const std::string& error_message) {
  LOG(ERROR) << "Shill Error: " << error_name << " : " << error_message;
}

bool DeviceDisabledScreenShown() {
  WizardController* const wizard_controller =
      WizardController::default_controller();
  EXPECT_TRUE(wizard_controller);
  return wizard_controller &&
         wizard_controller->current_screen() ==
             wizard_controller->GetScreen(DeviceDisabledScreenView::kScreenId);
}

}  // namespace

class DeviceDisablingTest
    : public OobeBaseTest,
      public NetworkStateInformer::NetworkStateInformerObserver {
 public:
  DeviceDisablingTest() = default;

  DeviceDisablingTest(const DeviceDisablingTest&) = delete;
  DeviceDisablingTest& operator=(const DeviceDisablingTest&) = delete;

  // Sets up a device state blob that indicates the device is disabled.
  void SetDeviceDisabledPolicy();

  // Sets up a device state blob that indicates the device is disabled, triggers
  // a policy plus device state fetch and waits for it to succeed.
  void MarkDisabledAndWaitForPolicyFetch();

  std::string GetCurrentScreenName(content::WebContents* web_contents);

 protected:
  // OobeBaseTest:
  void SetUpOnMainThread() override;

  // NetworkStateInformer::NetworkStateInformerObserver:
  void UpdateState(NetworkError::ErrorReason reason) override;

  std::unique_ptr<base::RunLoop> network_state_change_wait_run_loop_;
  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

void DeviceDisablingTest::SetDeviceDisabledPolicy() {
  // Prepare a policy fetch response that indicates the device is disabled.
  std::unique_ptr<ScopedDevicePolicyUpdate> policy_update =
      device_state_.RequestDevicePolicyUpdate();
  policy_update->policy_data()->mutable_device_state()->set_device_mode(
      enterprise_management::DeviceState::DEVICE_MODE_DISABLED);
}

void DeviceDisablingTest::MarkDisabledAndWaitForPolicyFetch() {
  base::RunLoop run_loop;
  // Set up an observer that will wait for the disabled setting to change.
  base::CallbackListSubscription subscription =
      CrosSettings::Get()->AddSettingsObserver(kDeviceDisabled,
                                               run_loop.QuitClosure());
  SetDeviceDisabledPolicy();
  // Trigger a policy fetch.
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  // Wait for the policy fetch to complete and the disabled setting to change.
  run_loop.Run();
}

void DeviceDisablingTest::SetUpOnMainThread() {
  network_state_change_wait_run_loop_ = std::make_unique<base::RunLoop>();

  OobeBaseTest::SetUpOnMainThread();

  // Set up fake networks.
  ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();
}

void DeviceDisablingTest::UpdateState(NetworkError::ErrorReason reason) {
  network_state_change_wait_run_loop_->Quit();
}

IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableDuringNormalOperation) {
  MarkDisabledAndWaitForPolicyFetch();
  // Check for WizardController state.
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();

  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

// Verifies that device disabling works when the ephemeral users policy is
// enabled. This case warrants its own test because the UI behaves somewhat
// differently when the policy is set: A background job runs on startup that
// causes the UI to try and show the login screen after some delay. It must
// be ensured that the login screen does not show and does not clobber the
// disabled screen.
IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableWithEphemeralUsers) {
  // Connect to the fake Ethernet network. This ensures that Chrome OS will not
  // try to show the offline error screen.
  base::RunLoop connect_run_loop;
  ShillServiceClient::Get()->Connect(dbus::ObjectPath("/service/eth1"),
                                     connect_run_loop.QuitClosure(),
                                     base::BindOnce(&ErrorCallbackFunction));
  connect_run_loop.Run();

  // Skip to the login screen.
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();

  // Mark the device as disabled and wait until cros settings update.
  MarkDisabledAndWaitForPolicyFetch();

  // Check for WizardController state.
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();

  // Disconnect from the fake Ethernet network.
  const LoginDisplayHost* host = LoginDisplayHost::default_host();
  OobeUI* const oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  const scoped_refptr<NetworkStateInformer> network_state_informer =
      oobe_ui->network_state_informer_for_test();
  ASSERT_TRUE(network_state_informer);
  network_state_informer->AddObserver(this);
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetectorMixin::NetworkStatus::kOffline);
  network_state_change_wait_run_loop_->Run();
  network_state_informer->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  // Verify that the offline error screen was not shown and the device disabled
  // screen is still being shown instead.
  // Check for WizardController state.
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();
}

class DeviceDisablingWithUsersTest : public DeviceDisablingTest {
 public:
  DeviceDisablingWithUsersTest() { login_manager_.AppendRegularUsers(2); }

 private:
  LoginManagerMixin login_manager_{&mixin_host_};
};

// Checks that OOBE dialog is not hidden when the device disabled screen is
// shown and "StartSignInScreen" is called.
IN_PROC_BROWSER_TEST_F(DeviceDisablingWithUsersTest, DialogNotHidden) {
  EXPECT_TRUE(ash::LoginScreenTestApi::ClickAddUserButton());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
  OobeScreenWaiter(GetFirstSigninScreen()).Wait();
  MarkDisabledAndWaitForPolicyFetch();
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();
  LoginDisplayHost::default_host()->StartSignInScreen();

  // Dialog should not be hidden.
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

// Sets the device disabled policy before the browser is started.
class PresetPolicyDeviceDisablingTest : public DeviceDisablingTest {
 public:
  PresetPolicyDeviceDisablingTest() = default;

  PresetPolicyDeviceDisablingTest(const PresetPolicyDeviceDisablingTest&) =
      delete;
  PresetPolicyDeviceDisablingTest& operator=(
      const PresetPolicyDeviceDisablingTest&) = delete;

 protected:
  // DeviceDisablingTest:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceDisablingTest::SetUpInProcessBrowserTestFixture();
    SetDeviceDisabledPolicy();
  }
};

// Same test as the one in DeviceDisablingTest, except the policy is being set
// before Chrome process is started. This test covers a crash (crbug.com/709518)
// in DeviceDisabledScreen where it would try to access DeviceDisablingManager
// even though it wasn't yet constructed fully.
IN_PROC_BROWSER_TEST_F(PresetPolicyDeviceDisablingTest,
                       DisableBeforeStartup) {
  EXPECT_TRUE(DeviceDisabledScreenShown());
  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

class DeviceDisablingBeforeLoginHostCreated
    : public PresetPolicyDeviceDisablingTest {
 public:
  DeviceDisablingBeforeLoginHostCreated() {
    // Start with user pods.
    login_mixin_.AppendManagedUsers(2);
  }

  bool SetUpUserDataDirectory() override {
    // LoginManagerMixin sets up command line in the SetUpUserDataDirectory.
    if (!PresetPolicyDeviceDisablingTest::SetUpUserDataDirectory())
      return false;
    // Postpone login host creation.
    base::CommandLine::ForCurrentProcess()->RemoveSwitch(
        switches::kForceLoginManagerInTests);
    return true;
  }

  bool ShouldWaitForOobeUI() override { return false; }

 protected:
  LoginManagerMixin login_mixin_{&mixin_host_};
};

// Sometimes LoginHost creation postponed (e.g. due to language switch
// https://crbug.com/1065569). This tests checks this flow.
IN_PROC_BROWSER_TEST_F(DeviceDisablingBeforeLoginHostCreated,
                       ShowsDisabledScreen) {
  EXPECT_TRUE(
      system::DeviceDisablingManager::IsDeviceDisabledDuringNormalOperation());
  EXPECT_EQ(nullptr, LoginDisplayHost::default_host());
  EXPECT_NE(nullptr,
            g_browser_process->platform_part()->device_disabling_manager());
  ShowLoginWizard(ash::OOBE_SCREEN_UNKNOWN);
  // Check for WizardController state.
  OobeScreenWaiter(DeviceDisabledScreenView::kScreenId).Wait();

  EXPECT_TRUE(ash::LoginScreenTestApi::IsOobeDialogVisible());
}

}  // namespace system
}  // namespace ash
