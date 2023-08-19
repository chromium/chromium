// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind.h"
#include "chrome/browser/ash/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/ash/login/app_mode/test/kiosk_apps_mixin.h"
#include "chrome/browser/ash/login/login_wizard.h"
#include "chrome/browser/ash/login/screens/error_screen.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/dialog_window_waiter.h"
#include "chrome/browser/ash/login/test/embedded_test_server_setup_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screens_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/shill_profile_client.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr char kWifiServiceName[] = "stub_wifi";
constexpr char kWifiNetworkName[] = "wifi-test-network";

const test::UIPath kErrorMessageGuestSigninLink = {"error-message",
                                                   "error-guest-signin-link"};

ErrorScreen* GetScreen() {
  return static_cast<ErrorScreen*>(
      WizardController::default_controller()->GetScreen(
          ErrorScreenView::kScreenId));
}

}  // namespace

class NetworkErrorScreenTest : public InProcessBrowserTest {
 public:
  NetworkErrorScreenTest() = default;

  NetworkErrorScreenTest(const NetworkErrorScreenTest&) = delete;
  NetworkErrorScreenTest& operator=(const NetworkErrorScreenTest&) = delete;

  ~NetworkErrorScreenTest() override = default;

  void SetUpOnMainThread() override {
    wizard_context_ = std::make_unique<WizardContext>();
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    InProcessBrowserTest::SetUpOnMainThread();

    ShowLoginWizard(WelcomeView::kScreenId);
    test::WaitForWelcomeScreen();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(
        switches::kDisableOOBEChromeVoxHintTimerForTesting);
    command_line->AppendSwitch(switches::kLoginManager);
  }

  void ShowErrorScreenWithNetworkList() {
    // The only reason we set UI state to UI_STATE_UPDATE is to show a list
    // of networks on the error screen. There are other UI states that show
    // the network list, picked one arbitrarily.
    GetScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);

    GetScreen()->Show(wizard_context_.get());

    // Wait until network list adds the wifi test network.
    OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

    EXPECT_TRUE(test::IsScanningRequestedOnErrorScreen());

    test::OobeJS()
        .CreateWaiter(WifiElementSelector(kWifiNetworkName) + " != null")
        ->Wait();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  std::string WifiElementSelector(const std::string& wifi_network_name) {
    return test::GetOobeElementPath(
               {"error-message", "offline-network-control", "networkSelect"}) +
           ".getNetworkListItemByNameForTest('" + wifi_network_name + "')";
  }

  void ClickOnWifiNetwork(const std::string& wifi_network_name) {
    test::OobeJS().Evaluate(WifiElementSelector(wifi_network_name) +
                            ".click()");
  }

  void SetUpDisconnectedWifiNetwork() {
    network_helper_->device_test()->ClearDevices();
    network_helper_->service_test()->ClearServices();

    network_helper_->device_test()->AddDevice(
        "/device/stub_wifi_device", shill::kTypeWifi, "stub_wifi_device");
    network_helper_->service_test()->AddService(
        kWifiServiceName, "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateIdle, true);
    network_helper_->service_test()->SetServiceProperty(
        kWifiServiceName, shill::kConnectableProperty, base::Value(true));
    network_helper_->profile_test()->AddService(
        ShillProfileClient::GetSharedProfilePath(), kWifiServiceName);

    // Network modification notifications are posted asynchronously. Wait until
    // idle to ensure observers are notified.
    base::RunLoop().RunUntilIdle();
  }

  std::unique_ptr<WizardContext> wizard_context_;

 private:
  std::unique_ptr<NetworkStateTestHelper> network_helper_;
};

// Test that the network list contains the fake wifi network.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, ShowsNetwork) {
  SetUpDisconnectedWifiNetwork();
  ShowErrorScreenWithNetworkList();

  test::OobeJS()
      .CreateWaiter(WifiElementSelector(kWifiNetworkName) + ".hidden == false")
      ->Wait();
}

// Test that error screen hides when a network is connected and that showing and
//  hiding the error screen does not modify WizardController's current_screen.
#if !defined(NDEBUG)
// Flaky timeout in debug build crbug.com/1132417.
#define MAYBE_SelectNetwork DISABLED_SelectNetwork
#else
#define MAYBE_SelectNetwork SelectNetwork
#endif
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, MAYBE_SelectNetwork) {
  SetUpDisconnectedWifiNetwork();
  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);

  ShowErrorScreenWithNetworkList();
  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);

  // Go back to welcome screen after hiding the error screen.
  GetScreen()->SetParentScreen(WelcomeView::kScreenId);
  ClickOnWifiNetwork(kWifiNetworkName);

  OobeScreenWaiter welecome_screen_waiter(WelcomeView::kScreenId);
  welecome_screen_waiter.set_assert_next_screen();
  welecome_screen_waiter.Wait();

  EXPECT_EQ(
      WizardController::default_controller()->current_screen()->screen_id(),
      WelcomeView::kScreenId);
}

// Test ConnectRequestCallback is called when connecting to a network.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, ConnectRequestCallback) {
  SetUpDisconnectedWifiNetwork();

  bool callback_called = false;
  auto subscription = GetScreen()->RegisterConnectRequestCallback(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  ShowErrorScreenWithNetworkList();
  ClickOnWifiNetwork(kWifiNetworkName);

  EXPECT_TRUE(callback_called);
}

// Test HideCallback is called after screen hides.
IN_PROC_BROWSER_TEST_F(NetworkErrorScreenTest, HideCallback) {
  bool callback_called = false;
  GetScreen()->SetHideCallback(
      base::BindLambdaForTesting([&]() { callback_called = true; }));

  GetScreen()->Show(wizard_context_.get());
  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  GetScreen()->Hide();

  EXPECT_TRUE(callback_called);
}

class GuestErrorScreenTest
    : public MixinBasedInProcessBrowserTest,
      public testing::WithParamInterface<DeviceStateMixin::State> {
 public:
  GuestErrorScreenTest() { login_manager_.set_session_restore_enabled(); }

  GuestErrorScreenTest(const GuestErrorScreenTest&) = delete;
  GuestErrorScreenTest& operator=(const GuestErrorScreenTest&) = delete;

  ~GuestErrorScreenTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
    SessionManagerClient::InitializeFakeInMemory();
    FakeSessionManagerClient::Get()->set_supports_browser_restart(true);
    wizard_context_ = std::make_unique<WizardContext>();
  }

 protected:
  std::unique_ptr<WizardContext> wizard_context_;
  LoginManagerMixin login_manager_{&mixin_host_};
  DeviceStateMixin device_state_{&mixin_host_, GetParam()};
};

// Test that guest signin option is shown when enabled and that clicking on it
// starts a guest session.
IN_PROC_BROWSER_TEST_P(GuestErrorScreenTest, PRE_GuestLogin) {
  GetScreen()->AllowGuestSignin(true);
  GetScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);
  GetScreen()->Show(wizard_context_.get());

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kErrorMessageGuestSigninLink);

  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  test::OobeJS().ClickOnPath(kErrorMessageGuestSigninLink);
  restart_job_waiter.Run();
}

IN_PROC_BROWSER_TEST_P(GuestErrorScreenTest, GuestLogin) {
  login_manager_.WaitForActiveSession();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

INSTANTIATE_TEST_SUITE_P(
    All,
    GuestErrorScreenTest,
    testing::Values(DeviceStateMixin::State::BEFORE_OOBE,
                    // We use OOBE completed and cloud enrolled to trigger the
                    // Gaia dialog right away.
                    DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED));

class KioskErrorScreenTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskErrorScreenTest() = default;

  KioskErrorScreenTest(const KioskErrorScreenTest&) = delete;
  KioskErrorScreenTest& operator=(const KioskErrorScreenTest&) = delete;

  ~KioskErrorScreenTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    skip_splash_wait_override_ =
        KioskLaunchController::SkipSplashScreenWaitForTesting();

    AddKioskAppToDevicePolicy();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);

    network_helper_->service_test()->AddService(
        kWifiServiceName, "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateIdle, /*visible=*/true);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 private:
  void AddKioskAppToDevicePolicy() {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    KioskAppsMixin::AppendKioskAccount(device_policy_update->policy_payload());
    device_policy_update.reset();

    device_state_.RequestDeviceLocalAccountPolicyUpdate(
        KioskAppsMixin::kEnterpriseKioskAccountId);
  }

  std::unique_ptr<NetworkStateTestHelper> network_helper_;

  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};
  KioskAppsMixin kiosk_apps_{&mixin_host_, embedded_test_server()};

  LoginManagerMixin login_manager_{&mixin_host_, {}};
};

// Verify that certificate manager dialog opens.
IN_PROC_BROWSER_TEST_F(KioskErrorScreenTest, OpenCertificateConfig) {
  KioskAppsMixin::WaitForAppsButton();
  EXPECT_TRUE(LoginScreenTestApi::IsAppsButtonShown());
  ASSERT_TRUE(LoginScreenTestApi::LaunchApp(KioskAppsMixin::kKioskAppId));

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  EXPECT_TRUE(LoginScreenTestApi::IsOobeDialogVisible());

  DialogWindowWaiter waiter(
      l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE));

  const test::UIPath kCertsButton = {"error-message", "configureCertsButton"};
  test::OobeJS().CreateVisibilityWaiter(true, kCertsButton)->Wait();
  test::OobeJS().ClickOnPath(kCertsButton);

  waiter.Wait();
}

}  // namespace ash
