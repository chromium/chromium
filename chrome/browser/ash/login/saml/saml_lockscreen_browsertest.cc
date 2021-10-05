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

    MixinBasedInProcessBrowserTest::SetUpOnMainThread();
  }

  void Login() { logged_in_user_mixin_.LogInUser(); }

  FakeSamlIdpMixin* fake_saml_idp() { return &fake_saml_idp_; }

  FakeGaiaMixin* fake_gaia_mixin() {
    return logged_in_user_mixin_.GetFakeGaiaMixin();
  }

 protected:
  base::test::ScopedFeatureList feature_list_;

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

  absl::optional<LockScreenReauthDialogTestHelper> lock_screen_reauth_dialog =
      LockScreenReauthDialogTestHelper::ShowDialogAndWait();
  ASSERT_TRUE(lock_screen_reauth_dialog);
  lock_screen_reauth_dialog->ForceSamlRedirect();

  // Expect the 'Verify Account' screen (the first screen the dialog shows) to
  // be visible and proceed to the SAML page.
  lock_screen_reauth_dialog->WaitForVerifyAccountScreen();
  lock_screen_reauth_dialog->ClickVerifyButton();

  lock_screen_reauth_dialog->WaitForSamlScreen();
  lock_screen_reauth_dialog->ExpectVerifyAccountScreenHidden();

  lock_screen_reauth_dialog->WaitForIdpPageLoad();

  // Fill-in the SAML IdP form and submit.
  test::JSChecker signin_frame_js = lock_screen_reauth_dialog->SigninFrameJS();
  signin_frame_js.CreateVisibilityWaiter(true, {"Email"})->Wait();
  signin_frame_js.TypeIntoPath(FakeGaiaMixin::kEnterpriseUser1, {"Email"});
  signin_frame_js.TypeIntoPath("actual_password", {"Password"});
  signin_frame_js.TapOn("Submit");

  ScreenLockerTester().WaitForUnlock();
}

}  // namespace ash
