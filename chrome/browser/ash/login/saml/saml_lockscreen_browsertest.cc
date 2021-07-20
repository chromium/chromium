// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/lock/screen_locker_tester.h"
#include "chrome/browser/ash/login/saml/fake_saml_idp_mixin.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager.h"
#include "chrome/browser/ash/login/saml/in_session_password_sync_manager_factory.h"
#include "chrome/browser/ash/login/session/user_session_manager_test_api.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/test_condition_waiter.h"
#include "chrome/browser/ash/login/users/test_users.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_reauth_dialogs.h"
#include "chrome/browser/ui/webui/chromeos/in_session_password_change/lock_screen_start_reauth_ui.h"
#include "chrome/browser/ui/webui/signin/signin_utils.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "chromeos/dbus/shill/fake_shill_manager_client.h"
#include "components/account_id/account_id.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_utils.h"
#include "net/dns/mock_host_resolver.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

const test::UIPath kSamlContainer = {"main-element", "body"};
const test::UIPath kMainVerifyButton = {"main-element",
                                        "nextButtonVerifyScreen"};
const test::UIPath kMainScreen = {"main-element", "verifyAccountScreen"};
const std::string kSigninFrame = "signin-frame";

constexpr char kTestAuthSIDCookie1[] = "fake-auth-SID-cookie-1";
constexpr char kTestAuthLSIDCookie1[] = "fake-auth-LSID-cookie-1";
constexpr char kTestRefreshToken[] = "fake-refresh-token";

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
        chromeos::FakeGaiaMixin::kEnterpriseUser1,
        fake_saml_idp_.GetSamlPageUrl());

    fake_gaia_mixin()->set_initialize_fake_merge_session(false);
    fake_gaia_mixin()->fake_gaia()->SetFakeMergeSessionParams(
        chromeos::FakeGaiaMixin::kEnterpriseUser1, kTestAuthSIDCookie1,
        kTestAuthLSIDCookie1);
    fake_gaia_mixin()->SetupFakeGaiaForLogin(
        chromeos::FakeGaiaMixin::kEnterpriseUser1, "", kTestRefreshToken);

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void Login() { logged_in_user_mixin_.LogInUser(); }

  void ShowDialogAndWait() {
    password_sync_manager_ =
        InSessionPasswordSyncManagerFactory::GetForProfile(
          ProfileManager::GetActiveUserProfile());
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
        prefs::kLockScreenReauthenticationEnabled, true);
    ASSERT_TRUE(password_sync_manager_);
    password_sync_manager_->CreateAndShowDialog();
    base::RunLoop().RunUntilIdle();

    // Fetch the dialog, WebUi controller and main message handler.
    reauth_dialog_ = password_sync_manager_->get_reauth_dialog_for_testing();
    ASSERT_TRUE(reauth_dialog_);
    reauth_webui_controller_ = static_cast<LockScreenStartReauthUI*>(
        reauth_dialog_->GetWebUIForTest()->GetController());
    ASSERT_TRUE(reauth_webui_controller_);
    main_handler_ = reauth_webui_controller_->GetMainHandlerForTests();
    ASSERT_TRUE(main_handler_);
    main_handler_->force_saml_redirect_for_testing();

    WaitForWebUi();
  }

  void LockscreenAndShowDialog() {
    Login();
    ScreenLockerTester().Lock();
    base::RunLoop().RunUntilIdle();
    ShowDialogAndWait();
  }

  void WaitForWebUi() {
    base::RunLoop run_loop;
    if (!main_handler_->IsJsReadyForTesting(run_loop.QuitClosure())) {
      run_loop.Run();
    }
  }

  void WaitForAuthenticatorToLoad() {
    base::RunLoop run_loop;
    if (!main_handler_->IsAuthenticatorLoaded(run_loop.QuitClosure())) {
      run_loop.Run();
    };
  }

  void WaitForIdpPageLoad() {
    content::DOMMessageQueue message_queue;
    content::ExecuteScriptAsync(
        reauth_dialog_->GetWebUIForTest()->GetWebContents(),
        R"($('main-element').authenticator_.addEventListener('authFlowChange',
            function f() {
              $('main-element').authenticator_.removeEventListener(
                  'authFlowChange', f);
              window.domAutomationController.send('Loaded');
            });)");
    std::string message;
    do {
      ASSERT_TRUE(message_queue.WaitForMessage(&message));
    } while (message != "\"Loaded\"");
  }

  test::JSChecker JS() {
    return test::JSChecker(reauth_dialog_->GetWebUIForTest()->GetWebContents());
  }

  test::JSChecker SigninFrameJS() {
    content::RenderFrameHost* frame = signin::GetAuthFrame(
        reauth_dialog_->GetWebUIForTest()->GetWebContents(), kSigninFrame);
    CHECK(frame && frame->IsDOMContentLoaded());
    test::JSChecker result = test::JSChecker(frame);
    return result;
  }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_; }

  FakeGaiaMixin* fake_gaia_mixin() {
    return logged_in_user_mixin_.GetFakeGaiaMixin();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  InSessionPasswordSyncManager* password_sync_manager_ = nullptr;
  chromeos::LockScreenStartReauthDialog* reauth_dialog_ = nullptr;
  LockScreenStartReauthUI* reauth_webui_controller_ = nullptr;
  LockScreenReauthHandler* main_handler_ = nullptr;

  chromeos::LoggedInUserMixin logged_in_user_mixin_{
      &mixin_host_,
      LoggedInUserMixin::LogInType::kRegular,
      embedded_test_server(),
      /*test_base=*/this,
      true /*should_launch_browser*/,
      AccountId::FromUserEmailGaiaId(
          chromeos::FakeGaiaMixin::kEnterpriseUser1,
          chromeos::FakeGaiaMixin::kEnterpriseUser1GaiaId),
      true /*include_initial_user*/};

  FakeSamlIdpMixin fake_saml_idp_{&mixin_host_, fake_gaia_mixin()};
};

// Flaky. See https://crbug.com/1224705.
IN_PROC_BROWSER_TEST_F(LockscreenWebUiTest, DISABLED_Login) {
  fake_saml_idp()->SetLoginHTMLTemplate("saml_login.html");

  LockscreenAndShowDialog();

  // Expect the main screen to be visible and proceed to the SAML page.
  JS().ExpectVisiblePath(kMainScreen);
  JS().TapOnPath(kMainVerifyButton);
  WaitForAuthenticatorToLoad();

  JS().ExpectVisiblePath(kSamlContainer);
  JS().ExpectHiddenPath(kMainScreen);
  base::RunLoop().RunUntilIdle();

  WaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  SigninFrameJS().CreateVisibilityWaiter(true, {"Email"})->Wait();
  SigninFrameJS().TypeIntoPath(chromeos::FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  SigninFrameJS().TypeIntoPath("actual_password", {"Password"});
  SigninFrameJS().TapOn("Submit");

  ScreenLockerTester().WaitForUnlock();
}

}  // namespace ash
