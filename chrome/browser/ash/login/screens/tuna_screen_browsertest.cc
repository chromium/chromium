// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/tuna_screen.h"

#include "ash/constants/ash_features.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/logged_in_user_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/tuna_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kNextButton[] = "nextButton";
constexpr char kBackButton[] = "backButton";
constexpr test::UIPath kNextButtonPath = {TunaScreenView::kScreenId.name,
                                          kNextButton};
constexpr test::UIPath kBackButtonPath = {TunaScreenView::kScreenId.name,
                                          kBackButton};

class TunaScreenTest : public OobeBaseTest {
 public:
  TunaScreenTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOobeAiIntro, features::kFeatureManagementOobeAiIntro,
         features::kOobeTuna, features::kFeatureManagementOobeTuna},
        {});
  }

  ~TunaScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Force the sync screen to be shown so that OOBE isn't destroyed
    // right after login due to all screens being skipped.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    TunaScreen* tuna_screen =
        WizardController::default_controller()->GetScreen<TunaScreen>();

    original_callback_ = tuna_screen->get_exit_callback_for_testing();
    tuna_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
  }

  TunaScreen::Result WaitForScreenExitResult() {
    TunaScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TestFuture<TunaScreen::Result> screen_result_waiter_;
  TunaScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_, {}, &fake_gaia_};
};

IN_PROC_BROWSER_TEST_F(TunaScreenTest, ForwardFlow) {
  login_manager_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      TunaScreenView::kScreenId);

  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), TunaScreen::Result::kNext);
}

IN_PROC_BROWSER_TEST_F(TunaScreenTest, BackwardFlow) {
  login_manager_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      TunaScreenView::kScreenId);

  test::OobeJS().TapOnPath(kBackButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), TunaScreen::Result::kBack);
  OobeScreenWaiter(AiIntroScreenView::kScreenId).Wait();
}

class TunaScreenChildTest : public TunaScreenTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    TunaScreenTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(TunaScreenChildTest, SkipScreenForChildUser) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  login_manager_.LoginAsNewChildUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      TunaScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(), TunaScreen::Result::kNotApplicable);
}

class TunaScreenManagedTest : public TunaScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(TunaScreenManagedTest, SkipScreenForManagedUser) {
  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();
  login_manager_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      TunaScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(), TunaScreen::Result::kNotApplicable);
}

}  // namespace
}  // namespace ash
