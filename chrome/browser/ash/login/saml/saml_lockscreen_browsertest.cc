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
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/network/network_connection_handler.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_handler_test_helper.h"
#include "chromeos/network/network_state_test_helper.h"
#include "components/account_id/account_id.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kWifiServicePath[] = "/service/wifi1";

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
      base::Value(shill::kStatePortal));

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
        base::Value(shill::kStatePortal));

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

}  // namespace ash
