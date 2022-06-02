// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/network_portal_detector_mixin.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kWifiServicePath[] = "/service/wifi1";
constexpr char kEthServicePath[] = "/service/eth1";

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

void SetDisconnected(const std::string& service_path) {
  base::RunLoop run_loop;
  ShillServiceClient::Get()->Disconnect(
      dbus::ObjectPath(service_path), run_loop.QuitWhenIdleClosure(),
      base::BindOnce(&ErrorCallbackFunction, run_loop.QuitClosure()));
  run_loop.Run();
}

}  // namespace

class LockscreenWebUiTest : public MixinBasedInProcessBrowserTest {
 public:
  LockscreenWebUiTest() {
    feature_list_.InitAndEnableFeature(
        features::kEnableSamlReauthenticationOnLockscreen);
  }

  LockscreenWebUiTest(const LockscreenWebUiTest&) = delete;
  LockscreenWebUiTest& operator=(const LockscreenWebUiTest&) = delete;

  ~LockscreenWebUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
    // TODO(crbug.com/1177416) - Fix this with a proper SSL solution.
    command_line->AppendSwitch(::switches::kIgnoreCertificateErrors);

    MixinBasedInProcessBrowserTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    ShillManagerClient::Get()->GetTestInterface()->SetupDefaultEnvironment();

    host_resolver()->AddRule("*", "127.0.0.1");

    test::UserSessionManagerTestApi session_manager_test_api(
        UserSessionManager::GetInstance());
    session_manager_test_api.SetShouldObtainTokenHandleInTests(false);

    fake_gaia_mixin()->fake_gaia()->RegisterSamlUser(
        FakeGaiaMixin::kEnterpriseUser1, fake_saml_idp_.GetSamlPageUrl());

    fake_gaia_mixin()->set_initialize_fake_merge_session(false);
    fake_gaia_mixin()->fake_gaia()->SetFakeMergeSessionParams(
        FakeGaiaMixin::kEnterpriseUser1, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);
    fake_gaia_mixin()->SetupFakeGaiaForLogin(FakeGaiaMixin::kEnterpriseUser1,
                                             "", kTestRefreshToken);

    // Set up fake networks.
    network_state_test_helper_ =
        std::make_unique<chromeos::NetworkStateTestHelper>(
            true /*use_default_devices_and_services*/);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { network_state_test_helper_.reset(); }

  void Login() { logged_in_user_mixin_.LogInUser(); }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_; }

  FakeGaiaMixin* fake_gaia_mixin() {
    return logged_in_user_mixin_.GetFakeGaiaMixin();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;

 private:
  LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(),
      /*test_base=*/this,
      true /*should_launch_browser*/,
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId),
      true /*include_initial_user*/};

  FakeSamlIdpMixin fake_saml_idp_{&mixin_host_, fake_gaia_mixin()};
};

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, Login) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);
  reauth_dialog_helper->ForceSamlRedirect();

  // Expect the 'Verify Account' screen (the first screen the dialog shows) to
  // be visible and proceed to the SAML page.
  reauth_dialog_helper->WaitForVerifyAccountScreen();
  reauth_dialog_helper->ClickVerifyButton();

  reauth_dialog_helper->WaitForSamlScreen();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();

  reauth_dialog_helper->WaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  ScreenLockerTester().WaitForUnlock();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, ShowNetworkDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  reauth_dialog_helper->ShowNetworkScreenAndWait();

  // Ensures that the web element 'cr-dialog' is really visible.
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Click on the actual button to close the dialog.
  reauth_dialog_helper->ClickCloseNetworkButton();
  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, TriggerDialogOnNetworkOff) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOffline, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Click on the actual button to close the dialog.
  reauth_dialog_helper->ClickCloseNetworkButton();
  // Ensures that both dialogs are closed.
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, TriggerAndHideNetworkDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOffline, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Reconnect network.
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOnline, /*visible=*/true);

  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();

  // Ensures that the network dialog is closed.
  reauth_dialog_helper->ExpectNetworkDialogHidden();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, CaptivePortal) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Disconnect from all networks in order to trigger the network screen.
  network_state_test_helper_->service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_state_test_helper_->service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOffline, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Change network to be behind a captive portal.
  network_state_test_helper_->service_test()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateRedirectFound));

  reauth_dialog_helper->WaitForCaptivePortalDialogToLoad();
  reauth_dialog_helper->WaitForCaptivePortalDialogToShow();
  reauth_dialog_helper->ExpectCaptivePortalDialogVisible();

  // User actions on captive portal page should lead to network becoming online,
  // so instead of mocking a portal page we simply switch the network state.
  network_state_test_helper_->service_test()->SetServiceProperty(
      kWifiServicePath, shill::kStateProperty,
      base::Value(shill::kStateOnline));

  reauth_dialog_helper->WaitForCaptivePortalDialogToClose();

  // Ensures that captive portal and network dialogs are closed.
  reauth_dialog_helper->ExpectCaptivePortalDialogHidden();
  reauth_dialog_helper->ExpectNetworkDialogHidden();
}

IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, TriggerAndHideCaptivePortalDialog) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // This test uses NetworkHandlerTestHelper instead of NetworkStateTestHelper
  // because closing captive portal dialog involves a call to
  // NetworkHandler::Get()
  NetworkHandlerTestHelper network_test_helper;
  network_test_helper.manager_test()->SetupDefaultEnvironment();

  // Disconnect from all networks in order to trigger the network screen.
  network_test_helper.service_test()->ClearServices();
  // TODO(crbug.com/1289309): make it clear what we are waiting for
  base::RunLoop().RunUntilIdle();
  network_test_helper.service_test()->AddService(
      /*service_path=*/kWifiServicePath, /*guid=*/kWifiServicePath,
      /*name=*/kWifiServicePath, /*type=*/shill::kTypeWifi,
      /*state=*/shill::kStateOffline, /*visible=*/true);

  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();

  reauth_dialog_helper->ExpectNetworkDialogVisible();

  auto TriggerAndCloseCaptivePortal = [&network_test_helper,
                                       &reauth_dialog_helper] {
    // Change network to be behind a captive portal.
    network_test_helper.service_test()->SetServiceProperty(
        kWifiServicePath, shill::kStateProperty,
        base::Value(shill::kStateRedirectFound));

    reauth_dialog_helper->WaitForCaptivePortalDialogToLoad();
    reauth_dialog_helper->WaitForCaptivePortalDialogToShow();
    reauth_dialog_helper->ExpectCaptivePortalDialogVisible();

    // Close captive portal dialog and check that we are back to network dialog
    reauth_dialog_helper->CloseCaptivePortalDialogAndWait();
    reauth_dialog_helper->ExpectCaptivePortalDialogHidden();
    reauth_dialog_helper->ExpectNetworkDialogVisible();
  };
  // Check that captive portal dialog can be opened and closed multiple times
  TriggerAndCloseCaptivePortal();
  TriggerAndCloseCaptivePortal();

  // Close all dialogs at the end of the test - otherwise these tests crash
  reauth_dialog_helper->ClickCloseNetworkButton();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();
}

// Sets up proxy server which requires authentication.
class ProxyAuthLockscreenWebUiTest : public LockscreenWebUiTest {
 public:
  ProxyAuthLockscreenWebUiTest()
      : proxy_server_(net::SpawnedTestServer::TYPE_BASIC_AUTH_PROXY,
                      base::FilePath()),
        login_handler_(nullptr) {}

  ProxyAuthLockscreenWebUiTest(const ProxyAuthLockscreenWebUiTest&) = delete;
  ProxyAuthLockscreenWebUiTest& operator=(const ProxyAuthLockscreenWebUiTest&) =
      delete;

  ~ProxyAuthLockscreenWebUiTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    LockscreenWebUiTest::SetUpCommandLine(command_line);
  }

  void SetUpOnMainThread() override {
    LockscreenWebUiTest::SetUpOnMainThread();

    // Disconnect unneeded wifi network - these tests use only the network which
    // corresponds to `kEthServicePath`
    SetDisconnected(kWifiServicePath);
    ConfigureNetworkBehindProxy();

    // Proxy authentication will be required as soon as we request any url from
    // lock screen's webview. This observer will notice it and allow us to
    // access corresponding `LoginHandler` object.
    auth_needed_observer_ =
        std::make_unique<content::WindowedNotificationObserver>(
            chrome::NOTIFICATION_AUTH_NEEDED,
            base::BindRepeating(&ProxyAuthLockscreenWebUiTest::OnAuthRequested,
                                base::Unretained(this)));
  }

  void SetUp() override {
    proxy_server_.set_redirect_connect_to_localhost(true);
    ASSERT_TRUE(proxy_server_.Start());
    LockscreenWebUiTest::SetUp();
  }

  void WaitForLoginHandler() { auth_needed_observer_->Wait(); }

  LoginHandler* login_handler() const { return login_handler_; }

 private:
  // Configure settings which are neccesarry for `NetworkStateInformer` to
  // report `NetworkStateInformer::PROXY_AUTH_REQUIRED` in the tests.
  void ConfigureNetworkBehindProxy() {
    network_portal_detector_.SetDefaultNetwork(
        kEthServicePath,
        NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_PROXY_AUTH_REQUIRED);

    base::Value proxy_config = ProxyConfigDictionary::CreateFixedServers(
        proxy_server_.host_port_pair().ToString(), "");

    ProxyConfigDictionary proxy_config_dict(std::move(proxy_config));
    const chromeos::NetworkState* network =
        network_state_test_helper_->network_state_handler()->DefaultNetwork();
    ASSERT_TRUE(network);
    ASSERT_EQ(network->guid(),
              FakeShillManagerClient::kFakeEthernetNetworkGuid);

    chromeos::proxy_config::SetProxyConfigForNetwork(proxy_config_dict,
                                                     *network);
    base::RunLoop().RunUntilIdle();
  }

  bool OnAuthRequested(const content::NotificationSource& /* source */,
                       const content::NotificationDetails& details) {
    login_handler_ =
        content::Details<LoginNotificationDetails>(details)->handler();
    return true;
  }

  NetworkPortalDetectorMixin network_portal_detector_{&mixin_host_};
  net::SpawnedTestServer proxy_server_;
  std::unique_ptr<content::WindowedNotificationObserver> auth_needed_observer_;
  // Used for proxy server authentication.
  LoginHandler* login_handler_;
};

IN_PROC_BROWSER_TEST_F(ProxyAuthLockscreenWebUiTest, SwitchToProxyNetwork) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Start with disconnected network.
  SetDisconnected(kEthServicePath);

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();
  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  reauth_dialog_helper->ForceSamlRedirect();

  // No networks are connected so we should start on the network screen.
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Connect to a network behind proxy.
  SetConnected(kEthServicePath);

  reauth_dialog_helper->ExpectNetworkDialogHidden();

  reauth_dialog_helper->WaitForVerifyAccountScreen();
  reauth_dialog_helper->ClickVerifyButton();

  reauth_dialog_helper->WaitForSamlScreen();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();

  // Wait for proxy login handler and authenticate.
  WaitForLoginHandler();
  ASSERT_TRUE(login_handler());
  ASSERT_EQ(login_handler()->web_contents()->GetOuterWebContents(),
            reauth_dialog_helper->DialogWebContents());
  login_handler()->SetAuth(u"foo", u"bar");

  reauth_dialog_helper->WaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  ScreenLockerTester().WaitForUnlock();
}

IN_PROC_BROWSER_TEST_F(ProxyAuthLockscreenWebUiTest, ProxyAuthCanBeCancelled) {
  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();
  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  reauth_dialog_helper->ForceSamlRedirect();

  reauth_dialog_helper->WaitForVerifyAccountScreen();
  reauth_dialog_helper->ClickVerifyButton();

  reauth_dialog_helper->WaitForSamlScreen();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();

  // Appearance of login handler means that proxy authentication was requested
  WaitForLoginHandler();
  ASSERT_TRUE(login_handler());
  ASSERT_EQ(login_handler()->web_contents()->GetOuterWebContents(),
            reauth_dialog_helper->DialogWebContents());

  content::WindowedNotificationObserver auth_cancelled_waiter(
      chrome::NOTIFICATION_AUTH_CANCELLED,
      content::NotificationService::AllSources());

  // Cancel proxy authentication
  login_handler()->CancelAuth();
  auth_cancelled_waiter.Wait();

  // Expect to end up on the network screen
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Close all dialogs at the end of the test - otherwise these tests crash
  reauth_dialog_helper->ClickCloseNetworkButton();
  reauth_dialog_helper->ExpectVerifyAccountScreenHidden();
}

}  // namespace ash
