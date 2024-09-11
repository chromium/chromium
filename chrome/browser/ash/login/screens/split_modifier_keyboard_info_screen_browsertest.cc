// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/split_modifier_keyboard_info_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
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
#include "chrome/browser/ui/webui/ash/login/split_modifier_keyboard_info_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "components/account_id/account_id.h"
#include "content/public/test/browser_test.h"

namespace ash {
namespace {

constexpr char kNextButton[] = "nextButton";
constexpr test::UIPath kNextButtonPath = {
    SplitModifierKeyboardInfoScreenView::kScreenId.name, kNextButton};

class SplitModifierKeyboardInfoScreenTest : public OobeBaseTest {
 public:
  SplitModifierKeyboardInfoScreenTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOobeSplitModifierKeyboardInfo}, {});
  }

  ~SplitModifierKeyboardInfoScreenTest() override = default;

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();
    // Force the sync screen to be shown so that OOBE isn't destroyed
    // right after login due to all screens being skipped.
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;

    SplitModifierKeyboardInfoScreen* split_modifier_screen =
        WizardController::default_controller()
            ->GetScreen<SplitModifierKeyboardInfoScreen>();

    original_callback_ = split_modifier_screen->get_exit_callback_for_testing();
    split_modifier_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kOobeSkipSplitModifierCheckForTesting);
  }

  SplitModifierKeyboardInfoScreen::Result WaitForScreenExitResult() {
    SplitModifierKeyboardInfoScreen::Result result =
        screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;

  base::test::TestFuture<SplitModifierKeyboardInfoScreen::Result>
      screen_result_waiter_;
  SplitModifierKeyboardInfoScreen::ScreenExitCallback original_callback_;

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_, {}, &fake_gaia_};
};

IN_PROC_BROWSER_TEST_F(SplitModifierKeyboardInfoScreenTest, ForwardFlow) {
  login_manager_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      SplitModifierKeyboardInfoScreenView::kScreenId);

  test::OobeJS().TapOnPath(kNextButtonPath);
  EXPECT_EQ(WaitForScreenExitResult(),
            SplitModifierKeyboardInfoScreen::Result::kNext);
}

class SplitModifierKeyboardInfoScreenChildTest
    : public SplitModifierKeyboardInfoScreenTest {
 public:
  // Child users require a user policy, set up an empty one so the user can
  // get through login.
  void SetUpInProcessBrowserTestFixture() override {
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    SplitModifierKeyboardInfoScreenTest::SetUpInProcessBrowserTestFixture();
  }

 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(SplitModifierKeyboardInfoScreenChildTest,
                       SkipScreenForChildUser) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->sign_in_as_child = true;
  login_manager_.LoginAsNewChildUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      SplitModifierKeyboardInfoScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(),
            SplitModifierKeyboardInfoScreen::Result::kNotApplicable);
}

class SplitModifierKeyboardInfoScreenManagedTest
    : public SplitModifierKeyboardInfoScreenTest {
 protected:
  const LoginManagerMixin::TestUserInfo test_user_{
      AccountId::FromUserEmailGaiaId("user@example.com", "1111")};
  UserPolicyMixin user_policy_mixin_{&mixin_host_, test_user_.account_id};
};

IN_PROC_BROWSER_TEST_F(SplitModifierKeyboardInfoScreenManagedTest,
                       SkipScreenForManagedUser) {
  // Mark user as managed.
  user_policy_mixin_.RequestPolicyUpdate();
  login_manager_.LoginWithDefaultContext(test_user_);
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      SplitModifierKeyboardInfoScreenView::kScreenId);
  EXPECT_EQ(WaitForScreenExitResult(),
            SplitModifierKeyboardInfoScreen::Result::kNotApplicable);
}

class SplitModifierKeyboardInfoScreenNoCapabilitiesTest
    : public SplitModifierKeyboardInfoScreenTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    // Not adding kOobeSkipSplitModifierCheckForTesting here to set-up case
    // when capabilities are not enabled.
  }
};

IN_PROC_BROWSER_TEST_F(SplitModifierKeyboardInfoScreenNoCapabilitiesTest,
                       SkipScreenWhenNoCapabilitiesEnabled) {
  login_manager_.LoginAsNewRegularUser();
  OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();

  WizardController::default_controller()->AdvanceToScreen(
      SplitModifierKeyboardInfoScreenView::kScreenId);

  EXPECT_EQ(WaitForScreenExitResult(),
            SplitModifierKeyboardInfoScreen::Result::kNotApplicable);
}

}  // namespace
}  // namespace ash
