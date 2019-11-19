// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_base_test.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/ui/login_display_host.h"
#include "chrome/browser/chromeos/login/ui/webui_login_view.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/ui/webui/chromeos/login/device_disabled_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/network_state_informer.h"
#include "chrome/browser/ui/webui/chromeos/login/oobe_ui.h"
#include "chrome/browser/ui/webui/chromeos/login/signin_screen_handler.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/dbus/shill/shill_manager_client.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/settings/cros_settings_names.h"
#include "components/policy/proto/device_management_backend.pb.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test_utils.h"
#include "dbus/object_path.h"

namespace chromeos {
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
  chromeos::DeviceStateMixin device_state_{
      &mixin_host_,
      chromeos::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  DISALLOW_COPY_AND_ASSIGN(DeviceDisablingTest);
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
  // Set up an |observer| that will wait for the disabled setting to change.
  std::unique_ptr<CrosSettings::ObserverSubscription> observer =
      CrosSettings::Get()->AddSettingsObserver(kDeviceDisabled,
                                               run_loop.QuitClosure());
  SetDeviceDisabledPolicy();
  // Trigger a policy fetch.
  FakeSessionManagerClient::Get()->OnPropertyChangeComplete(true);
  // Wait for the policy fetch to complete and the disabled setting to change.
  run_loop.Run();
}

std::string DeviceDisablingTest::GetCurrentScreenName(
    content::WebContents* web_contents ) {
  std::string screen_name;
  if (!content::ExecuteScriptAndExtractString(
          web_contents,
          "domAutomationController.send(Oobe.getInstance().currentScreen.id);",
          &screen_name)) {
    ADD_FAILURE();
  }
  return screen_name;
}

void DeviceDisablingTest::SetUpOnMainThread() {
  network_state_change_wait_run_loop_.reset(new base::RunLoop);

  OobeBaseTest::SetUpOnMainThread();

  // Set up fake networks.
  DBusThreadManager::Get()->GetShillManagerClient()->GetTestInterface()->
      SetupDefaultEnvironment();
}

void DeviceDisablingTest::UpdateState(NetworkError::ErrorReason reason) {
  network_state_change_wait_run_loop_->Quit();
}

IN_PROC_BROWSER_TEST_F(DeviceDisablingTest, DisableDuringNormalOperation) {
  MarkDisabledAndWaitForPolicyFetch();
  EXPECT_TRUE(DeviceDisabledScreenShown());
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
  DBusThreadManager::Get()->GetShillServiceClient()->Connect(
      dbus::ObjectPath("/service/eth1"),
      connect_run_loop.QuitClosure(),
      base::Bind(&ErrorCallbackFunction));
  connect_run_loop.Run();

  // Skip to the login screen.
  OobeScreenWaiter(GaiaView::kScreenId).Wait();

  // Mark the device as disabled and wait until cros settings update.
  MarkDisabledAndWaitForPolicyFetch();

  // When the ephemeral users policy is enabled, Chrome OS removes any non-owner
  // cryptohomes on startup. At the end of that process, JavaScript attempts to
  // show the login screen. Simulate this.
  const LoginDisplayHost* host = LoginDisplayHost::default_host();
  ASSERT_TRUE(host);
  content::WebContents* web_contents = host->GetOobeWebContents();
  ASSERT_TRUE(web_contents);
  ASSERT_TRUE(content::ExecuteScript(web_contents,
                                     "Oobe.showAddUserForTesting();"));

  // The login profile is scrubbed before attempting to show the login screen.
  // Wait for the scrubbing to finish.
  base::RunLoop run_loop;
  ProfileHelper::Get()->ClearSigninProfile(run_loop.QuitClosure());
  run_loop.Run();
  base::RunLoop().RunUntilIdle();

  // Verify that the login screen was not shown and the device disabled screen
  // is still being shown instead.
  EXPECT_EQ(DeviceDisabledScreenView::kScreenId.name,
            GetCurrentScreenName(web_contents));

  // Disconnect from the fake Ethernet network.
  OobeUI* const oobe_ui = host->GetOobeUI();
  ASSERT_TRUE(oobe_ui);
  const scoped_refptr<NetworkStateInformer> network_state_informer =
      oobe_ui->network_state_informer_for_test();
  ASSERT_TRUE(network_state_informer);
  network_state_informer->AddObserver(this);
  SigninScreenHandler* const signin_screen_handler =
      oobe_ui->signin_screen_handler();
  ASSERT_TRUE(signin_screen_handler);
  signin_screen_handler->SetOfflineTimeoutForTesting(
      base::TimeDelta::FromSeconds(0));
  network_portal_detector_.SimulateDefaultNetworkState(
      NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_OFFLINE);
  network_state_change_wait_run_loop_->Run();
  network_state_informer->RemoveObserver(this);
  base::RunLoop().RunUntilIdle();

  // Verify that the offline error screen was not shown and the device disabled
  // screen is still being shown instead.
  EXPECT_EQ(DeviceDisabledScreenView::kScreenId.name,
            GetCurrentScreenName(web_contents));
}

// Sets the device disabled policy before the browser is started.
class PresetPolicyDeviceDisablingTest : public DeviceDisablingTest {
 public:
  PresetPolicyDeviceDisablingTest() {}

 protected:
  // DeviceDisablingTest:
  void SetUpInProcessBrowserTestFixture() override {
    DeviceDisablingTest::SetUpInProcessBrowserTestFixture();
    SetDeviceDisabledPolicy();
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PresetPolicyDeviceDisablingTest);
};

// Same test as the one in DeviceDisablingTest, except the policy is being set
// before Chrome process is started. This test covers a crash (crbug.com/709518)
// in DeviceDisabledScreen where it would try to access DeviceDisablingManager
// even though it wasn't yet constructed fully.
IN_PROC_BROWSER_TEST_F(PresetPolicyDeviceDisablingTest,
                       DisableBeforeStartup) {
  EXPECT_TRUE(DeviceDisabledScreenShown());
}

}  // namespace system
}  // namespace chromeos
