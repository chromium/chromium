// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/update_required_screen.h"

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/json/json_writer.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/version_updater/version_updater.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/net/network_portal_detector_test_impl.h"
#include "chrome/browser/ash/policy/core/device_policy_builder.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/policy/handlers/minimum_version_policy_test_helpers.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/oobe_ui.h"
#include "chrome/browser/ui/webui/ash/login/update_required_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/dbus/dbus_thread_manager.h"
#include "chromeos/ash/components/dbus/update_engine/fake_update_engine_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "chromeos/dbus/constants/dbus_switches.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"
#include "ui/strings/grit/ui_strings.h"

namespace ash {
namespace {

namespace em = ::enterprise_management;

const test::UIPath kUpdateRequiredScreen = {"update-required"};
const test::UIPath kUpdateRequiredStep = {"update-required",
                                          "update-required-dialog"};
const test::UIPath kUpdateRequiredSubtitle = {"update-required",
                                              "update-subtitle"};
const test::UIPath kUpdateNowButton = {"update-required", "update-button"};
const test::UIPath kUpdateProcessStep = {"update-required",
                                         "downloadingUpdate"};
const test::UIPath kUpdateRequiredEolDialog = {"update-required", "eolDialog"};
const test::UIPath kEolAdminMessageContainer = {"update-required",
                                                "adminMessageContainer"};
const test::UIPath kEolAdminMessage = {"update-required", "adminMessage"};
const test::UIPath kEolDeleteUsersDataMessage = {"update-required",
                                                 "deleteDataMessageContainer"};
const test::UIPath kEolNoUsersDataMsg = {"update-required",
                                         "noUsersDataMessage"};
const test::UIPath kEolDeleteUsersDataLink = {"update-required",
                                              "deleteDataLink"};
const test::UIPath kEolDeleteUsersDataConfirmDialog = {"update-required",
                                                       "confirmationDialog"};
const test::UIPath kEolDeleteUsersDataConfirmButton = {"update-required",
                                                       "confirmDelete"};
const test::UIPath kEolDeleteUsersDataCancelButton = {"update-required",
                                                      "cancelDelete"};
const test::UIPath kMeteredNetworkStep = {"update-required",
                                          "update-need-permission-dialog"};
const test::UIPath kMeteredNetworkAcceptButton = {
    "update-required", "cellular-permission-accept-button"};
const test::UIPath kNoNetworkStep = {"update-required",
                                     "update-required-no-network-dialog"};

// Elements in downloadingUpdate
const test::UIPath kUpdateProcessCheckingStep = {
    "update-required", "downloadingUpdate", "checking-for-updates-dialog"};
const test::UIPath kUpdateProcessUpdatingStep = {
    "update-required", "downloadingUpdate", "updating-dialog"};
const test::UIPath kUpdateProcessCompleteStep = {
    "update-required", "downloadingUpdate", "update-complete-dialog"};
const test::UIPath kCheckingForUpdatesMessage = {
    "update-required", "downloadingUpdate", "checkingForUpdatesMsg"};
const test::UIPath kUpdatingProgress = {"update-required", "downloadingUpdate",
                                        "updating-progress"};

constexpr char kWifiServicePath[] = "/service/wifi2";
constexpr char kCellularServicePath[] = "/service/cellular1";
constexpr char kDemoEolMessage[] = "Please return your device.";
constexpr char16_t kDomain16[] = u"example.com";
constexpr char kManager[] = "user@example.com";
constexpr char16_t kManager16[] = u"user@example.com";

OobeUI* GetOobeUI() {
  auto* host = LoginDisplayHost::default_host();
  return host ? host->GetOobeUI() : nullptr;
}

void ErrorCallbackFunction(base::OnceClosure run_loop_quit_closure,
                           const std::string& error_name,
                           const std::string& error_message) {
  std::move(run_loop_quit_closure).Run();
  FAIL() << "Shill Error: " << error_name << " : " << error_message;
}

void SetConnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Connect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

void WaitForConfirmationDialogToOpen() {
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kEolDeleteUsersDataConfirmDialog) +
                    ".open")
      ->Wait();
}

void WaitForConfirmationDialogToClose() {
  test::OobeJS()
      .CreateWaiter(test::GetOobeElementPath(kEolDeleteUsersDataConfirmDialog) +
                    ".open === false")
      ->Wait();
}

class UpdateRequiredScreenTest : public OobeBaseTest {
 public:
  UpdateRequiredScreenTest() { login_manager_mixin_.AppendRegularUsers(2); }
  ~UpdateRequiredScreenTest() override = default;
  UpdateRequiredScreenTest(const UpdateRequiredScreenTest&) = delete;
  UpdateRequiredScreenTest& operator=(const UpdateRequiredScreenTest&) = delete;

  // OobeBaseTest:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(chromeos::switches::kShillStub,
                                    "clear=1, cellular=1, wifi=1");
  }
  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    // Set up fake networks.
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/true);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);
  }
  void TearDownOnMainThread() override {
    network_state_test_helper_.reset();

    OobeBaseTest::TearDownOnMainThread();
  }

  void SetUpdateEngineStatus(update_engine::Operation operation) {
    update_engine::StatusResult status;
    status.set_current_operation(operation);
    update_engine_client()->set_default_status(status);
    update_engine_client()->NotifyObserversThatStatusChanged(status);
  }

  void SetNetworkState(const std::string& service_path,
                       const std::string& state) {
    network_state_test_helper_->service_test()->SetServiceProperty(
        service_path, shill::kStateProperty, base::Value(state));
  }

  void ShowUpdateRequiredScreen() {
    LoginDisplayHost::default_host()->StartWizard(
        UpdateRequiredView::kScreenId);
    OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
    update_screen_waiter.set_assert_next_screen();
    update_screen_waiter.Wait();
    test::OobeJS().ExpectVisiblePath(kUpdateRequiredScreen);
  }

  void SetEolMessageAndWaitForSettingsChange(std::string eol_message) {
    policy::DevicePolicyBuilder* const device_policy(
        policy_helper_.device_policy());
    em::ChromeDeviceSettingsProto& proto(device_policy->payload());
    proto.mutable_device_minimum_version_aue_message()->set_value(eol_message);
    policy_helper_.RefreshPolicyAndWaitUntilDeviceSettingsUpdated(
        {kDeviceMinimumVersionAueMessage});
  }

 protected:
  raw_ptr<UpdateRequiredScreen> update_required_screen_;
  // Error screen - owned by OobeUI.
  // Version updater - owned by `update_required_screen_`.
  raw_ptr<VersionUpdater> version_updater_ = nullptr;

  // Handles network connections
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;
  policy::DevicePolicyCrosTestHelper policy_helper_;
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestCaptivePortal) {
  ShowUpdateRequiredScreen();

  network_state_test_helper_->ResetDevicesAndServices();
  std::string wifi_path =
      network_state_test_helper_->ConfigureWiFi(shill::kStateRedirectFound);

  network_portal_detector::InitializeForTesting(
      new NetworkPortalDetectorTestImpl());
  network_portal_detector::GetInstance()->Enable();

  static_cast<UpdateRequiredScreen*>(
      WizardController::default_controller()->current_screen())
      ->SetErrorMessageDelayForTesting(base::Milliseconds(10));

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredStep);

  // Click update button to trigger the update process.
  test::OobeJS().ClickOnPath(kUpdateNowButton);

  // If the network is a captive portal network, the error message is shown.
  OobeScreenWaiter error_screen_waiter(ErrorScreenView::kScreenId);
  error_screen_waiter.set_assert_next_screen();
  error_screen_waiter.Wait();

  ErrorScreen* error_screen = GetOobeUI()->GetErrorScreen();
  EXPECT_EQ(UpdateRequiredView::kScreenId.AsId(),
            error_screen->GetParentScreen());
  test::OobeJS().ExpectVisible("error-message");
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "captive-portal-message-text"});
  test::OobeJS().ExpectVisiblePath(
      {"error-message", "captive-portal-proxy-message-text"});

  // If network goes back online, the error screen should be hidden and update
  // process should start.
  network_state_test_helper_->SetServiceProperty(
      wifi_path, shill::kStateProperty, base::Value(shill::kStateOnline));
  EXPECT_EQ(OOBE_SCREEN_UNKNOWN.AsId(), error_screen->GetParentScreen());

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredScreen);
  test::OobeJS().ExpectVisiblePath(kUpdateProcessStep);
  network_portal_detector::Shutdown();
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolReached) {
  update_engine_client()->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
  test::OobeJS().ExpectVisiblePath(kEolDeleteUsersDataMessage);
  test::OobeJS().ExpectHiddenPath(kEolNoUsersDataMsg);
}

// Test to verify that clicking on the confirm button on the popup in case of
// update required and end-of-life reached, deletes all users on the device.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolDeleteUsersConfirm) {
  EXPECT_EQ(user_manager::UserManager::Get()->GetUsers().size(), 2u);
  update_engine_client()->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectVisiblePath(kEolDeleteUsersDataMessage);

  test::OobeJS().TapOnPath(kEolDeleteUsersDataLink);
  WaitForConfirmationDialogToOpen();

  test::OobeJS().TapOnPath(kEolDeleteUsersDataConfirmButton);
  WaitForConfirmationDialogToClose();

  test::OobeJS().CreateVisibilityWaiter(true, kEolNoUsersDataMsg)->Wait();
  test::OobeJS().ExpectHiddenPath(kEolDeleteUsersDataMessage);
  EXPECT_EQ(user_manager::UserManager::Get()->GetUsers().size(), 0u);
}

// Test to verify that clicking on the cancel button on the popup in case of
// update required and end-of-life reached, does not delete any user.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolDeleteUsersCancel) {
  EXPECT_EQ(user_manager::UserManager::Get()->GetUsers().size(), 2u);
  update_engine_client()->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectVisiblePath(kEolDeleteUsersDataMessage);

  test::OobeJS().TapOnPath(kEolDeleteUsersDataLink);
  WaitForConfirmationDialogToOpen();

  test::OobeJS().TapOnPath(kEolDeleteUsersDataCancelButton);
  WaitForConfirmationDialogToClose();

  test::OobeJS().ExpectVisiblePath(kEolDeleteUsersDataMessage);
  test::OobeJS().ExpectHiddenPath(kEolNoUsersDataMsg);
  EXPECT_EQ(user_manager::UserManager::Get()->GetUsers().size(), 2u);
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolReachedAdminMessage) {
  update_engine_client()->set_eol_date(
      base::DefaultClock::GetInstance()->Now() - base::Days(1));
  SetEolMessageAndWaitForSettingsChange(kDemoEolMessage);
  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectVisiblePath(kEolAdminMessageContainer);
  test::OobeJS().ExpectElementText(kDemoEolMessage, kEolAdminMessage);
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
}

IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestEolNotReached) {
  update_engine_client()->set_eol_date(
      base::DefaultClock::GetInstance()->Now() + base::Days(1));
  ShowUpdateRequiredScreen();

  test::OobeJS().ExpectHiddenPath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectVisiblePath(kUpdateRequiredStep);
}

// This tests the state of update required screen when the device is initially
// connected to a metered network and the user grants permission to update over
// it.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateOverMeteredNetwork) {
  // Disconnect Wifi network.
  SetNetworkState(kWifiServicePath, shill::kStateIdle);
  // Connect to cellular network and show update required screen.
  SetConnected(kCellularServicePath);

  ShowUpdateRequiredScreen();

  // Screen prompts user to either connect to a non-metered network or start
  // update over current metered network.
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
  test::OobeJS().ExpectVisiblePath(kMeteredNetworkStep);

  // Click to start update over metered network.
  test::OobeJS().TapOnPath(kMeteredNetworkAcceptButton);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateProcessStep)->Wait();

  // Expect screen to show progress of the update process.
  test::OobeJS().ExpectHiddenPath(kMeteredNetworkStep);
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the state of update required screen when the device is initially
// not connected to any network and the user connects to Wifi to show update
// required screen.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateRequiredNoNetwork) {
  // Disconnect from all networks and show update required screen.
  network_state_test_helper_->service_test()->ClearServices();
  base::RunLoop().RunUntilIdle();

  ShowUpdateRequiredScreen();

  // Screen shows user to connect to a network to start update.
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
  test::OobeJS().ExpectVisiblePath(kNoNetworkStep);

  // Connect to a WiFi network.
  network_state_test_helper_->service_test()->AddService(
      kWifiServicePath, kWifiServicePath, kWifiServicePath /* name */,
      shill::kTypeWifi, shill::kStateOnline, true);

  // Update required screen is shown when user moves from no network to a good
  // network.
  test::OobeJS().CreateVisibilityWaiter(true, kUpdateRequiredStep)->Wait();
}

// This tests the condition when the user switches to a metered network during
// the update process. The user then grants the permission to continue the
// update.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest,
                       TestUpdateProcessNeedPermission) {
  // Wifi is connected, show update required screen.
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectVisiblePath(kUpdateRequiredStep);

  // Click to start update process.
  test::OobeJS().ClickOnPath(kUpdateNowButton);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateProcessStep)->Wait();

  // Expect screen to show progress of the update process.
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
  test::OobeJS().ExpectVisiblePath(kUpdateProcessStep);

  // Network changed to a metered network and update engine requires permission
  // to continue.
  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  SetUpdateEngineStatus(update_engine::Operation::NEED_PERMISSION_TO_UPDATE);

  test::OobeJS().CreateVisibilityWaiter(true, kMeteredNetworkStep)->Wait();

  test::OobeJS().ExpectHiddenPath(kUpdateProcessStep);

  // Screen prompts user to continue update on metered network. Click to
  // continue.
  test::OobeJS().TapOnPath(kMeteredNetworkAcceptButton);
  // Update process resumes.
  test::OobeJS().CreateVisibilityWaiter(true, kUpdateProcessStep)->Wait();

  test::OobeJS().ExpectHiddenPath(kMeteredNetworkStep);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the state of update required screen when the device is initially
// connected to a metered network and the update process starts automatically on
// switching to a non metered network.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest,
                       TestMeteredNetworkToGoodNetwork) {
  // Disconnect from Wifi and connect to cellular network.
  SetNetworkState(kWifiServicePath, shill::kStateIdle);
  SetConnected(kCellularServicePath);

  ShowUpdateRequiredScreen();

  // Screen prompts user to either connect to a non-metered network or start
  // update over current metered network.
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);
  test::OobeJS().ExpectVisiblePath(kMeteredNetworkStep);

  // Connect to a WiFi network and update starts automatically.
  SetNetworkState(kWifiServicePath, shill::kStateOnline);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateProcessStep)->Wait();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredScreen);
  test::OobeJS().ExpectHiddenPath(kMeteredNetworkStep);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  SetUpdateEngineStatus(update_engine::Operation::UPDATE_AVAILABLE);
  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// This tests the update process initiated from update required screen.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestUpdateProcess) {
  // Wifi is connected, show update required screen.
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectVisiblePath(kUpdateRequiredStep);

  // Click to start update process.
  test::OobeJS().ClickOnPath(kUpdateNowButton);

  test::OobeJS().CreateVisibilityWaiter(true, kUpdateProcessStep)->Wait();
  test::OobeJS().ExpectHiddenPath(kUpdateRequiredStep);

  SetUpdateEngineStatus(update_engine::Operation::CHECKING_FOR_UPDATE);
  // Wait for the content of the dialog to be rendered.
  test::OobeJS()
      .CreateDisplayedWaiter(true, kCheckingForUpdatesMessage)
      ->Wait();
  test::OobeJS().ExpectVisiblePath(kUpdateProcessCheckingStep);
  test::OobeJS().ExpectHiddenPath(kUpdateProcessUpdatingStep);
  test::OobeJS().ExpectHiddenPath(kUpdateProcessCompleteStep);

  SetUpdateEngineStatus(update_engine::Operation::DOWNLOADING);
  // Wait for the content of the dialog to be rendered.
  test::OobeJS().CreateDisplayedWaiter(true, kUpdatingProgress)->Wait();
  test::OobeJS().ExpectHiddenPath(kUpdateProcessCheckingStep);

  SetUpdateEngineStatus(update_engine::Operation::UPDATED_NEED_REBOOT);
  test::OobeJS()
      .CreateVisibilityWaiter(true, kUpdateProcessCompleteStep)
      ->Wait();
  test::OobeJS().ExpectHiddenPath(kUpdateProcessUpdatingStep);

  // UpdateStatusChanged(status) calls RebootAfterUpdate().
  EXPECT_EQ(1, update_engine_client()->reboot_after_update_call_count());
}

// Validates that the manager presented to the user in the subtitle is the
// domain to which the device belongs.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenTest, TestSubtitle) {
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(IDS_UPDATE_REQUIRED_SCREEN_MESSAGE, kDomain16,
                                ui::GetChromeOSDeviceName()),
      kUpdateRequiredSubtitle);
}

class UpdateRequiredScreenFlexOrgTest : public UpdateRequiredScreenTest {
 public:
  UpdateRequiredScreenFlexOrgTest() = default;
  ~UpdateRequiredScreenFlexOrgTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    UpdateRequiredScreenTest::SetUpInProcessBrowserTestFixture();
    policy_helper_.device_policy()->policy_data().set_managed_by(kManager);
    policy_helper_.RefreshDevicePolicy();
  }

 protected:
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

// For FlexOrgs, validates that the manager presented to the user in the
// subtitle is the manager of the FlexOrg.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenFlexOrgTest, TestSubtitle) {
  ShowUpdateRequiredScreen();
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringFUTF8(IDS_UPDATE_REQUIRED_SCREEN_MESSAGE, kManager16,
                                ui::GetChromeOSDeviceName()),
      kUpdateRequiredSubtitle);
}

class UpdateRequiredScreenPolicyPresentTest : public OobeBaseTest {
 public:
  UpdateRequiredScreenPolicyPresentTest() = default;
  ~UpdateRequiredScreenPolicyPresentTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    OobeBaseTest::SetUpInProcessBrowserTestFixture();
    // Create and set policy value.
    SetAndRefreshMinimumChromeVersionPolicy(
        policy::CreateMinimumVersionSingleRequirementPolicyValue(
            "1111.2.3.4" /* version */, 0 /* warning */, 0 /* eol_warning */,
            false /* unmanaged_user_restricted */));
    // Simulate end-of-life reached.
    update_engine_client()->set_eol_date(
        base::DefaultClock::GetInstance()->Now() - base::Days(1));
  }

  void SetAndRefreshMinimumChromeVersionPolicy(base::Value::Dict value) {
    policy::DevicePolicyBuilder* const device_policy(
        policy_helper_.device_policy());
    em::ChromeDeviceSettingsProto& proto(device_policy->payload());
    std::string policy_value;
    EXPECT_TRUE(base::JSONWriter::Write(value, &policy_value));
    proto.mutable_device_minimum_version()->set_value(policy_value);
    policy_helper_.RefreshDevicePolicy();
  }

 protected:
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
  policy::DevicePolicyCrosTestHelper policy_helper_;
};

// Test to verify that reboot after deleting all users data from the device
// still shows the update required screen to block user sign in.
IN_PROC_BROWSER_TEST_F(UpdateRequiredScreenPolicyPresentTest,
                       TestUpdateRequiredScreen) {
  EXPECT_EQ(user_manager::UserManager::Get()->GetUsers().size(), 0u);
  OobeScreenWaiter update_screen_waiter(UpdateRequiredView::kScreenId);
  update_screen_waiter.set_assert_next_screen();
  update_screen_waiter.Wait();

  test::OobeJS().ExpectVisiblePath(kUpdateRequiredEolDialog);
  test::OobeJS().ExpectVisiblePath(kEolNoUsersDataMsg);
  test::OobeJS().ExpectHiddenPath(kEolDeleteUsersDataMessage);
}

}  // namespace
}  // namespace ash
