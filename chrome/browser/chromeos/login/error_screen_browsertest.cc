// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/login_screen_test_api.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/bind_test_util.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/chromeos/app_mode/fake_cws.h"
#include "chrome/browser/chromeos/login/app_mode/kiosk_launch_controller.h"
#include "chrome/browser/chromeos/login/login_wizard.h"
#include "chrome/browser/chromeos/login/screens/error_screen.h"
#include "chrome/browser/chromeos/login/test/device_state_mixin.h"
#include "chrome/browser/chromeos/login/test/dialog_window_waiter.h"
#include "chrome/browser/chromeos/login/test/embedded_test_server_mixin.h"
#include "chrome/browser/chromeos/login/test/js_checker.h"
#include "chrome/browser/chromeos/login/test/login_manager_mixin.h"
#include "chrome/browser/chromeos/login/test/oobe_screen_waiter.h"
#include "chrome/browser/chromeos/login/wizard_context.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/app_launch_splash_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/chromeos/login/welcome_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"
#include "chromeos/dbus/shill/shill_profile_client.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/policy/proto/chrome_device_policy.pb.h"
#include "components/user_manager/user_manager.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace em = enterprise_management;

namespace chromeos {

namespace {

// This is a simple test app that creates an app window and immediately closes
// it again. Webstore data json is in
//   chrome/test/data/chromeos/app_mode/webstore/inlineinstall/
//       detail/ggaeimfdpnmlhdhpcikgoblffmkckdmn
constexpr char kTestKioskAppId[] = "ggaeimfdpnmlhdhpcikgoblffmkckdmn";
constexpr char kTestKioskAccountId[] = "enterprise-kiosk-app@localhost";

constexpr char kWifiServiceName[] = "stub_wifi";
constexpr char kWifiNetworkName[] = "wifi-test-network";

ErrorScreen* GetScreen() {
  return static_cast<ErrorScreen*>(
      WizardController::default_controller()->GetScreen(
          ErrorScreenView::kScreenId));
}

}  // namespace

class NetworkErrorScreenTest : public InProcessBrowserTest {
 public:
  NetworkErrorScreenTest() = default;
  ~NetworkErrorScreenTest() override = default;

  void SetUpOnMainThread() override {
    wizard_context_ = std::make_unique<WizardContext>();
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);
    InProcessBrowserTest::SetUpOnMainThread();

    ShowLoginWizard(WelcomeView::kScreenId);
    OobeScreenWaiter(WelcomeView::kScreenId).Wait();
  }

  void ShowErrorScreenWithNetworkList() {
    // The only reason we set UI state to UI_STATE_UPDATE is to show a list
    // of networks on the error screen. There are other UI states that show
    // the network list, picked one arbitrarily.
    GetScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);

    GetScreen()->Show(wizard_context_.get());

    // Wait until network list adds the wifi test network.
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
    return base::StrCat(
        {"$('offline-network-control').$$('#networkSelect')"
         ".getNetworkListItemByNameForTest('",
         wifi_network_name, "')"});
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

  DISALLOW_COPY_AND_ASSIGN(NetworkErrorScreenTest);
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

class GuestErrorScreenTest : public MixinBasedInProcessBrowserTest {
 public:
  GuestErrorScreenTest() { login_manager_.set_session_restore_enabled(); }
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

 private:
  DISALLOW_COPY_AND_ASSIGN(GuestErrorScreenTest);
};

// Test that guest signin option is shown when enabled and that clicking on it
// starts a guest session.
IN_PROC_BROWSER_TEST_F(GuestErrorScreenTest, PRE_GuestLogin) {
  GetScreen()->AllowGuestSignin(true);
  GetScreen()->SetUIState(NetworkError::UI_STATE_UPDATE);
  GetScreen()->Show(wizard_context_.get());

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath({"error-guest-signin-link"});

  base::RunLoop restart_job_waiter;
  FakeSessionManagerClient::Get()->set_restart_job_callback(
      restart_job_waiter.QuitClosure());

  test::OobeJS().ClickOnPath({"error-guest-signin-link"});
  restart_job_waiter.Run();
}

IN_PROC_BROWSER_TEST_F(GuestErrorScreenTest, GuestLogin) {
  login_manager_.WaitForActiveSession();
  user_manager::UserManager* user_manager = user_manager::UserManager::Get();
  EXPECT_TRUE(user_manager->IsLoggedInAsGuest());
}

class KioskErrorScreenTest : public MixinBasedInProcessBrowserTest {
 public:
  KioskErrorScreenTest() = default;
  ~KioskErrorScreenTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    host_resolver()->AddRule("*", "127.0.0.1");

    skip_splash_wait_override_ =
        KioskLaunchController::SkipSplashScreenWaitForTesting();
    network_wait_override_ = KioskLaunchController::SetNetworkWaitForTesting(
        base::TimeDelta::FromSeconds(0));

    fake_cws_.Init(embedded_test_server());
    fake_cws_.SetUpdateCrx(kTestKioskAppId,
                           base::StrCat({kTestKioskAppId, ".crx"}), "1.0.0");

    AddKioskAppToDevicePolicy();

    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();
  }

  void SetUpOnMainThread() override {
    network_helper_ = std::make_unique<NetworkStateTestHelper>(
        /*use_default_devices_and_services=*/false);

    network_helper_->service_test()->AddService(
        kWifiServiceName, "wifi_guid", kWifiNetworkName, shill::kTypeWifi,
        shill::kStateOffline, /*visible=*/true);

    apps_loaded_waiter_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_KIOSK_APPS_LOADED,
            content::NotificationService::AllSources());

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    network_helper_.reset();
    apps_loaded_waiter_.reset();
    MixinBasedInProcessBrowserTest::TearDownOnMainThread();
  }

 protected:
  content::WindowedNotificationObserver* apps_loaded_waiter() {
    return apps_loaded_waiter_.get();
  }

 private:
  void AddKioskAppToDevicePolicy() {
    std::unique_ptr<ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    em::DeviceLocalAccountsProto* const device_local_accounts =
        device_policy_update->policy_payload()->mutable_device_local_accounts();

    em::DeviceLocalAccountInfoProto* const account =
        device_local_accounts->add_account();
    account->set_account_id(kTestKioskAccountId);
    account->set_type(em::DeviceLocalAccountInfoProto::ACCOUNT_TYPE_KIOSK_APP);
    account->mutable_kiosk_app()->set_app_id(kTestKioskAppId);

    device_policy_update.reset();

    std::unique_ptr<ScopedUserPolicyUpdate> device_local_account_policy_update =
        device_state_.RequestDeviceLocalAccountPolicyUpdate(
            kTestKioskAccountId);
    device_local_account_policy_update.reset();
  }

  std::unique_ptr<NetworkStateTestHelper> network_helper_;

  std::unique_ptr<content::WindowedNotificationObserver> apps_loaded_waiter_;

  std::unique_ptr<base::AutoReset<bool>> skip_splash_wait_override_;
  std::unique_ptr<base::AutoReset<base::TimeDelta>> network_wait_override_;

  FakeCWS fake_cws_;

  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

  EmbeddedTestServerSetupMixin embedded_test_server_setup_{
      &mixin_host_, embedded_test_server()};

  LoginManagerMixin login_manager_{&mixin_host_, {}};

  DISALLOW_COPY_AND_ASSIGN(KioskErrorScreenTest);
};

// Verify that certificate manager dialog opens.
// Disabled for being flaky. See crbug.com/1116058.
IN_PROC_BROWSER_TEST_F(KioskErrorScreenTest, DISABLED_OpenCertificateConfig) {
  apps_loaded_waiter()->Wait();
  EXPECT_TRUE(ash::LoginScreenTestApi::LaunchApp(kTestKioskAppId));

  OobeScreenWaiter(ErrorScreenView::kScreenId).Wait();

  DialogWindowWaiter waiter(
      l10n_util::GetStringUTF16(IDS_CERTIFICATE_MANAGER_TITLE));

  test::OobeJS().TapOnPath({"error-message-md-configure-certs-button"});
  waiter.Wait();
}

}  // namespace chromeos
