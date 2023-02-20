// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/ranges/algorithm.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/saml/lockscreen_reauth_dialog_test_helper.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/ash/policy/affiliation/affiliation_test_helper.h"
#include "chrome/browser/ash/policy/core/device_policy_cros_browser_test.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/ui/login/login_handler.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/ash/components/dbus/session_manager/fake_session_manager_client.h"
#include "chromeos/ash/components/dbus/shill/fake_shill_manager_client.h"
#include "chromeos/ash/components/network/network_connection_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_handler_callbacks.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_state_test_helper.h"
#include "chromeos/ash/components/network/proxy/proxy_config_handler.h"
#include "components/account_id/account_id.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/proxy_config/proxy_config_dictionary.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "google_apis/gaia/gaia_urls.h"
#include "net/cookies/canonical_cookie.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestAuthSIDCookie2[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie2[] = "fake-auth-LSID-cookie-1";
constexpr char kTestRefreshToken[] = "fake-refresh-token";
constexpr char kWifiServicePath[] = "/service/wifi1";
constexpr char kEthServicePath[] = "/service/eth1";

constexpr char kSAMLIdPCookieName[] = "saml";
constexpr char kSAMLIdPCookieValue[] = "value";
constexpr char kAffiliationID[] = "test id";

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
  LockscreenWebUiTest() = default;
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
    network_state_test_helper_ = std::make_unique<NetworkStateTestHelper>(
        true /*use_default_devices_and_services*/);
    network_state_test_helper_->manager_test()->SetupDefaultEnvironment();
    // Fake networks have been set up. Connect to WiFi network.
    SetConnected(kWifiServicePath);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override { network_state_test_helper_.reset(); }

  void Login() {
    logged_in_user_mixin_.LogInUser();
    // Because the `logged_in_user_mixin_` uses a stub authenticator, we need to
    // also configure the fake UserDataAuth, otherwise lock-screen flow fails.
    cryptohome_mixin_.MarkUserAsExisting(GetAccountId());
  }

  void LoginWithoutUpdatingPolicies() {
    logged_in_user_mixin_.LogInUser(/*issue_any_scope_token=*/false,
                                    /*wait_for_active_session=*/true,
                                    /*request_policy_update=*/false);
    // Because the `logged_in_user_mixin_` uses a stub authenticator, we need to
    // also configure the fake UserDataAuth, otherwise lock-screen flow fails.
    cryptohome_mixin_.MarkUserAsExisting(GetAccountId());
  }

  AccountId GetAccountId() { return logged_in_user_mixin_.GetAccountId(); }

  // Go through online authentication (with saml) flow on the lock screen.
  void UnlockWithSAML() {
    absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
        LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

    // Fill-in the SAML IdP form and submit.
    test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
    signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
    signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
    signin_frame_js.TypeIntoPath("actual_password", {"Password"});
    signin_frame_js.TapOn("Submit");

    // Ensures that the re-auth dialog is closed.
    reauth_dialog_helper->WaitForReauthDialogToClose();
    ScreenLockerTester().WaitForUnlock();
  }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_; }

  FakeGaiaMixin* fake_gaia_mixin() {
    return logged_in_user_mixin_.GetFakeGaiaMixin();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;
  std::unique_ptr<NetworkStateTestHelper> network_state_test_helper_;

 private:
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
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

// Test Lockscreen reauth main flow.
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, Login) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  UnlockWithSAML();
}

// Tests that we can switch from SAML page to GAIA page on the lock screen.
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, SamlSwitchToGaia) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  reauth_dialog_helper->ClickChangeIdPButtonOnSamlScreen();

  reauth_dialog_helper->ExpectGaiaScreenVisible();
}

// Tests the cancel button in Verify Screen.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_VerifyScreenCancel DISABLED_VerifyScreenCancel
#else
#define MAYBE_VerifyScreenCancel VerifyScreenCancel
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_VerifyScreenCancel) {
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
  reauth_dialog_helper->ClickCancelButtonOnVerifyScreen();

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ASSERT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());

  // Verify that the dialog can be opened again.
  LockScreenReauthDialogTestHelper::ShowDialogAndWait();
}

// Tests the close button in SAML Screen.
// TODO(crbug.com/1401612): re-enable this test. Flakily times out on
// linux-chromeos-rel.
#if BUILDFLAG(IS_CHROMEOS)
#define MAYBE_SamlScreenCancel DISABLED_SamlScreenCancel
#else
#define MAYBE_SamlScreenCancel SamlScreenCancel
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_SamlScreenCancel) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  reauth_dialog_helper->ClickCancelButtonOnSamlScreen();

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ASSERT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());

  // Verify that the dialog can be opened again.
  LockScreenReauthDialogTestHelper::ShowDialogAndWait();
}

// Tests the single password scraped flow.
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, ScrapedSingle) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  content::DOMMessageQueue message_queue(
      reauth_dialog_helper->DialogWebContents());

  // Make sure that the password is scraped correctly.
  ASSERT_TRUE(content::ExecuteScript(
      reauth_dialog_helper->DialogWebContents(),
      "$('main-element').authenticator_.addEventListener('authCompleted',"
      "    function(e) {"
      "      var password = e.detail.password;"
      "      window.domAutomationController.send(password);"
      "    });"));

  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();

  // Fill-in the SAML IdP form and submit.
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"Password"});

  // Scraping a single password should finish the login and start the session.
  signin_frame_js.TapOn("Submit");

  std::string message;
  do {
    ASSERT_TRUE(message_queue.WaitForMessage(&message));
  } while (message != "\"fake_password\"");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests password scraping from a dynamically created password field.
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, ScrapedDynamic) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.Evaluate(
      "(function() {"
      "  var newPassInput = document.createElement('input');"
      "  newPassInput.id = 'DynamicallyCreatedPassword';"
      "  newPassInput.type = 'password';"
      "  newPassInput.name = 'Password';"
      "  document.forms[0].appendChild(newPassInput);"
      "})();");

  // Fill-in the SAML IdP form and submit.
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"DynamicallyCreatedPassword"});

  // Scraping a single password should finish the login and start the session.
  signin_frame_js.TapOn("Submit");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests the multiple password scraped flow.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ScrapedMultiple DISABLED_ScrapedMultiple
#else
#define MAYBE_ScrapedMultiple ScrapedMultiple
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_ScrapedMultiple) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_two_passwords.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("fake_password", {"Password"});
  signin_frame_js.TypeIntoPath("password1", {"Password1"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectSamlScreenHidden();
  reauth_dialog_helper->ExpectPasswordConfirmInputHidden();

  // Entering an unknown password should go back to the confirm password screen.
  reauth_dialog_helper->SendConfirmPassword("wrong_password");
  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectPasswordConfirmInputHidden();

  // Either scraped password should be able to sign-in.
  reauth_dialog_helper->SendConfirmPassword("password1");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Test when no password is scraped.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ScrapedNone DISABLED_ScrapedNone
#else
#define MAYBE_ScrapedNone ScrapedNone
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_ScrapedNone) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login_no_passwords.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.TypeIntoPath("fake_user", {"Email"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectSamlScreenHidden();
  reauth_dialog_helper->ExpectPasswordConfirmInputVisible();

  // Entering passwords that don't match will make us land again in the same
  // page.
  reauth_dialog_helper->SetManualPasswords("Test1", "Test2");
  reauth_dialog_helper->ExpectSamlConfirmPasswordVisible();
  reauth_dialog_helper->ExpectPasswordConfirmInputVisible();

  // Two matching passwords should let the user to authenticate.
  reauth_dialog_helper->SetManualPasswords("Test1", "Test1");

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// Tests another account is authenticated other than the one used in sign
// in.
// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_VerifyAgainFlow DISABLED_VerifyAgainFlow
#else
#define MAYBE_VerifyAgainFlow VerifyAgainFlow
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_VerifyAgainFlow) {
  fake_gaia_mixin()->fake_gaia()->SetFakeMergeSessionParams(
      FakeGaiaMixin::kEnterpriseUser2, kTestAuthSIDCookie1,
      kTestAuthLSIDCookie1);

  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  Login();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::StartSamlAndWaitForIdpPageLoad();

  // Authenticate in the IdP with another account other than the one used in
  // sign in.
  fake_gaia_mixin()->fake_gaia()->SetFakeMergeSessionParams(
      FakeGaiaMixin::kEnterpriseUser2, kTestAuthSIDCookie2,
      kTestAuthLSIDCookie2);
  test::JSChecker signin_frame_js = reauth_dialog_helper->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser2, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  reauth_dialog_helper->ExpectErrorScreenVisible();
  reauth_dialog_helper->ClickCancelButtonOnErrorScreen();

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();

  ASSERT_TRUE(session_manager::SessionManager::Get()->IsScreenLocked());
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ShowNetworkDialog DISABLED_ShowNetworkDialog
#else
#define MAYBE_ShowNetworkDialog ShowNetworkDialog
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_ShowNetworkDialog) {
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

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_TriggerDialogOnNetworkOff DISABLED_TriggerDialogOnNetworkOff
#else
#define MAYBE_TriggerDialogOnNetworkOff TriggerDialogOnNetworkOff
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_TriggerDialogOnNetworkOff) {
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
      /*state=*/shill::kStateIdle, /*visible=*/true);

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
      /*state=*/shill::kStateIdle, /*visible=*/true);

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
      /*state=*/shill::kStateIdle, /*visible=*/true);

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

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_TriggerAndHideCaptivePortalDialog \
  DISABLED_TriggerAndHideCaptivePortalDialog
#else
#define MAYBE_TriggerAndHideCaptivePortalDialog \
  TriggerAndHideCaptivePortalDialog
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest,
                       MAYBE_TriggerAndHideCaptivePortalDialog) {
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
      /*state=*/shill::kStateIdle, /*visible=*/true);

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

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_LoadAbort DISABLED_LoadAbort
#else
#define MAYBE_LoadAbort LoadAbort
#endif
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, MAYBE_LoadAbort) {
  Login();

  // Make gaia landing page unreachable
  fake_gaia_mixin()->fake_gaia()->SetFixedResponse(
      GaiaUrls::GetInstance()->embedded_setup_chromeos_url(2),
      net::HTTP_NOT_FOUND);

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  absl::optional<LockScreenReauthDialogTestHelper> reauth_dialog_helper =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(reauth_dialog_helper);

  // Unreachable gaia page should have resulted in load abort error which should
  // trigger the network dialog
  reauth_dialog_helper->WaitForNetworkDialogAndSetHandlers();
  reauth_dialog_helper->ExpectNetworkDialogVisible();

  // Close dialog at the end of the test - otherwise test will crash on exit
  reauth_dialog_helper->ClickCloseNetworkButton();
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
    ProxyConfigDictionary proxy_config_dict(
        ProxyConfigDictionary::CreateFixedServers(
            proxy_server_.host_port_pair().ToString(), ""));
    const NetworkState* network =
        network_state_test_helper_->network_state_handler()->DefaultNetwork();
    ASSERT_TRUE(network);
    ASSERT_EQ(network->guid(),
              FakeShillManagerClient::kFakeEthernetNetworkGuid);

    proxy_config::SetProxyConfigForNetwork(proxy_config_dict, *network);
    base::RunLoop().RunUntilIdle();
  }

  bool OnAuthRequested(const content::NotificationSource& /* source */,
                       const content::NotificationDetails& details) {
    login_handler_ =
        content::Details<LoginNotificationDetails>(details)->handler();
    return true;
  }

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

  // Ensures that the re-auth dialog is closed.
  reauth_dialog_helper->WaitForReauthDialogToClose();
  ScreenLockerTester().WaitForUnlock();
}

// TODO(crbug.com/1414002): Flaky on ChromeOS MSAN.
#if defined(MEMORY_SANITIZER)
#define MAYBE_ProxyAuthCanBeCancelled DISABLED_ProxyAuthCanBeCancelled
#else
#define MAYBE_ProxyAuthCanBeCancelled ProxyAuthCanBeCancelled
#endif
IN_PROC_BROWSER_TEST_F(ProxyAuthLockscreenWebUiTest,
                       MAYBE_ProxyAuthCanBeCancelled) {
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

// Fixture which allows to test transfer of saml cookies during online
// reauthentication on the lock screen.
class SAMLCookieTransferTest : public LockscreenWebUiTest {
 public:
  SAMLCookieTransferTest() {
    device_state_.set_skip_initial_policy_setup(true);
  }

  SAMLCookieTransferTest(const SAMLCookieTransferTest&) = delete;
  SAMLCookieTransferTest& operator=(const SAMLCookieTransferTest&) = delete;

  ~SAMLCookieTransferTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();
    LockscreenWebUiTest::SetUpInProcessBrowserTestFixture();

    policy::DevicePolicyCrosTestHelper device_policy_test_helper;
    // Enable DeviceTransferSAMLCookies policy.
    device_policy_test_helper.device_policy()
        ->payload()
        .mutable_saml_settings()
        ->set_transfer_saml_cookies(true);
    // Make user affiliated - this is another condition required to transfer
    // saml cookies.
    const std::set<std::string> device_affiliation_ids = {kAffiliationID};
    auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
        FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper, device_affiliation_ids)));
    policy::UserPolicyBuilder user_policy_builder;
    ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetUserAffiliationIDs(
        &user_policy_builder, GetAccountId(), device_affiliation_ids)));
  }

  // Add some random cookie to user partition. This is needed because during
  // online reauthentication on the lock screen we transfer cookies only if it
  // is a subsequent login. To detect "subsequent login", we check if user's
  // cookie jar is not empty. These tests do not simulate online authentication
  // on sign-in screen which is why we need to add a cookie manually with this
  // method.
  void AddCookieToUserPartition() {
    constexpr char kRandomCookieName[] = "random_cookie";
    constexpr char kRandomCookieValue[] = "random_cookie_value";

    Profile* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    ASSERT_TRUE(profile);
    net::CookieOptions options;
    options.set_include_httponly();
    profile->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->SetCanonicalCookie(
            *net::CanonicalCookie::CreateSanitizedCookie(
                fake_saml_idp()->GetSamlPageUrl(), kRandomCookieName,
                kRandomCookieValue, ".example.com", /*path=*/std::string(),
                /*creation_time=*/base::Time(),
                /*expiration_time=*/base::Time(),
                /*last_access_time=*/base::Time(), /*secure=*/true,
                /*http_only=*/false, net::CookieSameSite::NO_RESTRICTION,
                net::COOKIE_PRIORITY_DEFAULT, /*same_party=*/false,
                /*partition_key=*/absl::nullopt),
            fake_saml_idp()->GetSamlPageUrl(), options, base::DoNothing());
    ExpectCookieInUserProfile(kRandomCookieName, kRandomCookieValue);
  }

  void ExpectCookieInUserProfile(const std::string& cookie_name,
                                 const std::string& cookie_value) {
    Profile* profile = ProfileHelper::Get()->GetProfileByUser(
        user_manager::UserManager::Get()->GetActiveUser());
    net::CookieList cookie_list_;
    base::RunLoop run_loop;
    profile->GetDefaultStoragePartition()
        ->GetCookieManagerForBrowserProcess()
        ->GetAllCookies(base::BindLambdaForTesting(
            [&](const std::vector<net::CanonicalCookie>& cookies) {
              cookie_list_ = cookies;
              run_loop.Quit();
            }));
    run_loop.Run();
    EXPECT_GT(cookie_list_.size(), 0u);

    const auto saml_cookie_iterator = base::ranges::find(
        cookie_list_, cookie_name,
        [](const net::CanonicalCookie& cookie) { return cookie.Name(); });
    EXPECT_NE(saml_cookie_iterator, cookie_list_.end());
    EXPECT_EQ(cookie_value, saml_cookie_iterator->Value());
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Test transfer of saml cookies during online reauth on the lock screen
IN_PROC_BROWSER_TEST_F(SAMLCookieTransferTest, CookieTransfer) {
  fake_saml_idp()->SetCookieValue(kSAMLIdPCookieValue);
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  LoginWithoutUpdatingPolicies();
  AddCookieToUserPartition();

  ScreenLockerTester().Lock();

  UnlockWithSAML();

  ExpectCookieInUserProfile(kSAMLIdPCookieName, kSAMLIdPCookieValue);
}

// Fixture which sets SAML SSO profile to device policy protobuff
class SamlSsoProfileTest : public LockscreenWebUiTest {
 public:
  SamlSsoProfileTest() { device_state_.set_skip_initial_policy_setup(true); }

  SamlSsoProfileTest(const SamlSsoProfileTest&) = delete;
  SamlSsoProfileTest& operator=(const SamlSsoProfileTest&) = delete;

  ~SamlSsoProfileTest() override = default;

  void SetUpInProcessBrowserTestFixture() override {
    SessionManagerClient::InitializeFakeInMemory();
    LockscreenWebUiTest::SetUpInProcessBrowserTestFixture();

    // Set sso profile to device policy protobuff. It will be fetched from there
    // during online reauth.
    policy::DevicePolicyCrosTestHelper device_policy_test_helper;
    device_policy_test_helper.device_policy()->policy_data().set_sso_profile(
        fake_saml_idp()->GetIdpSsoProfile());

    // Set affiliation and user policies - this is needed for login in tests to
    // work correctly
    const std::set<std::string> device_affiliation_ids = {kAffiliationID};
    auto affiliation_helper = policy::AffiliationTestHelper::CreateForCloud(
        FakeSessionManagerClient::Get());
    ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetDeviceAffiliationIDs(
        &device_policy_test_helper, device_affiliation_ids)));
    policy::UserPolicyBuilder user_policy_builder;
    ASSERT_NO_FATAL_FAILURE((affiliation_helper.SetUserAffiliationIDs(
        &user_policy_builder, GetAccountId(), device_affiliation_ids)));
  }

 private:
  DeviceStateMixin device_state_{
      &mixin_host_, DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};
};

// Test that during online reauth on the lock screen we can perform saml
// redirection based on sso profile.
IN_PROC_BROWSER_TEST_F(SamlSsoProfileTest, ReauthBasedOnSsoProfile) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  // Set wrong redirect url for domain-based saml redirection. This ensures that
  // for test to finish successfully it should perform redirection based on sso
  // profile.
  const GURL wrong_redirect_url("https://wrong.com");
  fake_gaia_mixin()->fake_gaia()->RegisterSamlDomainRedirectUrl(
      fake_saml_idp()->GetIdpDomain(), wrong_redirect_url);

  LoginWithoutUpdatingPolicies();

  // Lock the screen and trigger the lock screen SAML reauth dialog.
  ScreenLockerTester().Lock();

  UnlockWithSAML();
}

}  // namespace ash
