// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/ai_intro_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/ai_intro_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gemini_intro_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kNextButton[] = "nextButton";
constexpr test::UIPath kNextButtonPath = {AiIntroScreenView::kScreenId.name,
                                          kNextButton};

class AiIntroScreenTest : public OobeBaseTest {
 public:
  AiIntroScreenTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kFeatureManagementOobeAiIntro,
         features::kFeatureManagementOobeGeminiIntro,
         features::kOobeGeminiIntroForTesting},
        {});
  }

  ~AiIntroScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Force the sync screen to be shown so that OOBE isn't destroyed
    // right after login due to all screens being skipped.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    AiIntroScreen* ai_intro_screen =
        WizardController::default_controller()->GetScreen<AiIntroScreen>();

    original_callback_ = ai_intro_screen->get_exit_callback_for_testing();
    ai_intro_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
  }

  AiIntroScreen::Result WaitForScreenExitResult() {
    AiIntroScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TestFuture<AiIntroScreen::Result> screen_result_waiter_;
  AiIntroScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_, {}, &fake_gaia_};
};

IN_PROC_BROWSER_TEST_F(AiIntroScreenTest, ForwardFlow) {
  login_manager_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      AiIntroScreenView::kScreenId);

  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(), AiIntroScreen::Result::kNext);
  OobeScreenWaiter(GeminiIntroScreenView::kScreenId).Wait();
}

class AiIntroScreenChildTest : public AiIntroScreenTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    AiIntroScreenTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(AiIntroScreenChildTest, SkipScreenForChildUser) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  login_manager_.LoginAsNewChildUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      AiIntroScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(), AiIntroScreen::Result::kNotApplicable);
}

class AiIntroScreenManagedTest : public AiIntroScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(AiIntroScreenManagedTest, SkipScreenForManagedUser) {
  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();
  login_manager_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      AiIntroScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(), AiIntroScreen::Result::kNotApplicable);
}

}  // namespace
}  // namespace ash
