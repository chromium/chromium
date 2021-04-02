// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/fake_gaia_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/local_policy_test_server_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/pin_setup_screen_handler.h"
#include "chromeos/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/login/auth/stub_authenticator_builder.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

using ::testing::ElementsAre;

namespace chromeos {
namespace {

const test::UIPath kBackButton = {"pin-setup", "backButton"};
const test::UIPath kNextButton = {"pin-setup", "nextButton"};
const test::UIPath kSkipButton = {"pin-setup", "setupSkipButton"};
const test::UIPath kDoneButton = {"pin-setup", "doneButton"};
const test::UIPath kPinKeyboardInput = {"pin-setup", "pinKeyboard",
                                        "pinKeyboard", "pinInput"};

}  // namespace

class PinSetupScreenTest
    : public OobeBaseTest,
      public testing::WithParamInterface<user_manager::UserType> {
 public:
  PinSetupScreenTest() {
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::Get()->set_supports_low_entropy_credentials(false);

    if (GetParam() == user_manager::USER_TYPE_CHILD) {
      fake_gaia_ =
          std::make_unique<FakeGaiaMixin>(&mixin_host_, embedded_test_server());
      policy_server_ =
          std::make_unique<LocalPolicyTestServerMixin>(&mixin_host_);
      user_policy_mixin_ = std::make_unique<UserPolicyMixin>(
          &mixin_host_, test_child_user_.account_id, policy_server_.get());
    }
  }
  ~PinSetupScreenTest() override = default;

  void SetUpOnMainThread() override {
    PinSetupScreen* screen = static_cast<PinSetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            PinSetupScreenView::kScreenId));
    original_callback_ = screen->get_exit_callback_for_testing();
    screen->set_exit_callback_for_testing(base::BindRepeating(
        &PinSetupScreenTest::HandleScreenExit, base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void SetUpInProcessBrowserTestFixture() override {
    // Child users require a user policy, set up an empty one so the user can
    // get through login.
    if (GetParam() == user_manager::USER_TYPE_CHILD)
      ASSERT_TRUE(user_policy_mixin_->RequestPolicyUpdate());

    OobeBaseTest::SetUpInProcessBrowserTestFixture();
  }

  void LogIn() {
    if (GetParam() == user_manager::USER_TYPE_CHILD) {
      UserContext user_context =
          LoginManagerMixin::CreateDefaultUserContext(test_child_user_);
      user_context.SetRefreshToken(FakeGaiaMixin::kFakeRefreshToken);
      fake_gaia_->SetupFakeGaiaForChildUser(
          test_child_user_.account_id.GetUserEmail(),
          test_child_user_.account_id.GetGaiaId(),
          FakeGaiaMixin::kFakeRefreshToken, true /*issue_any_scope_token*/);
      login_manager_mixin_.AttemptLoginUsingAuthenticator(
          user_context,
          std::make_unique<StubAuthenticatorBuilder>(user_context));
    } else {
      login_manager_mixin_.LoginAsNewRegularUser();
    }
  }

  void EnterPin() { test::OobeJS().TypeIntoPath("654321", kPinKeyboardInput); }

  void ShowPinSetupScreen() {
    // Force the sync screen to be shown so that we don't jump to PIN setup
    // screen (consuming auth session) in unbranded build
    auto autoreset = WizardController::ForceBrandedBuildForTesting(true);

    LogIn();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          PinSetupScreenView::kScreenId);
    }
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(PinSetupScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  base::Optional<PinSetupScreen::Result> screen_result_;
  base::HistogramTester histogram_tester_;

 private:
  void HandleScreenExit(PinSetupScreen::Result result) {
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  PinSetupScreen::ScreenExitCallback original_callback_;
  bool screen_exited_ = false;
  base::RepeatingClosure screen_exit_callback_;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};

  // Used for child account test.
  const LoginManagerMixin::TestUserInfo test_child_user_{
      AccountId::FromUserEmailGaiaId("user@test.com", "123456789"),
      user_manager::USER_TYPE_CHILD};
  std::unique_ptr<FakeGaiaMixin> fake_gaia_;
  std::unique_ptr<LocalPolicyTestServerMixin> policy_server_;
  std::unique_ptr<UserPolicyMixin> user_policy_mixin_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PinSetupScreenTest,
                         ::testing::Values(user_manager::USER_TYPE_REGULAR,
                                           user_manager::USER_TYPE_CHILD));

IN_PROC_BROWSER_TEST_P(PinSetupScreenTest, Skipped) {
  ShowPinSetupScreen();

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::NOT_APPLICABLE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 0);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 0);
}

IN_PROC_BROWSER_TEST_P(PinSetupScreenTest, SkipOnStart) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ShowPinSetupScreen();
  WaitForScreenShown();

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Skipped", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(
              PinSetupScreen::UserAction::kSkipButtonClickedOnStart),
          1)));
}

IN_PROC_BROWSER_TEST_P(PinSetupScreenTest, SkipInFlow) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ShowPinSetupScreen();
  WaitForScreenShown();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

  test::OobeJS().TapOnPath(kSkipButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::USER_SKIP);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Skipped", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(
              PinSetupScreen::UserAction::kSkipButtonClickedInFlow),
          1)));
}

IN_PROC_BROWSER_TEST_P(PinSetupScreenTest, FinishedFlow) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ShowPinSetupScreen();
  WaitForScreenShown();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kDoneButton})->Wait();

  test::OobeJS().TapOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::DONE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(PinSetupScreen::UserAction::kDoneButtonClicked),
          1)));
}

// Tests the PIN setup screen in the scenario when the device supports low
// entropy credentials and PIN can be used for login.
class PinForLoginSetupScreenTest : public PinSetupScreenTest {
 protected:
  PinForLoginSetupScreenTest() {
    // Enable PIN for login (overrides base class setting).
    UserDataAuthClient::InitializeFake();
    FakeUserDataAuthClient::Get()->set_supports_low_entropy_credentials(true);
    scoped_feature_list_.InitAndEnableFeature(features::kPinSetupForFamilyLink);
  }

  ~PinForLoginSetupScreenTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(All,
                         PinForLoginSetupScreenTest,
                         ::testing::Values(user_manager::USER_TYPE_REGULAR,
                                           user_manager::USER_TYPE_CHILD));

// Tests that PIN setup is shown to Family Link users (but not to regular users)
// on clamshell devices that support low entropy credentials.
IN_PROC_BROWSER_TEST_P(PinForLoginSetupScreenTest, ClamshellMode) {
  ShowPinSetupScreen();

  if (GetParam() == user_manager::USER_TYPE_CHILD) {
    WaitForScreenShown();

    EnterPin();
    test::OobeJS().TapOnPath(kNextButton);
    test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

    EnterPin();
    test::OobeJS().TapOnPath(kNextButton);
    test::OobeJS().CreateVisibilityWaiter(true, {kDoneButton})->Wait();

    test::OobeJS().TapOnPath(kDoneButton);

    WaitForScreenExit();
    EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::DONE);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 1);
    histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
    EXPECT_THAT(
        histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
        ElementsAre(base::Bucket(
            static_cast<int>(PinSetupScreen::UserAction::kDoneButtonClicked),
            1)));
  } else {
    WaitForScreenExit();
    EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::NOT_APPLICABLE);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 0);
    histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 0);
  }
}

// Tests that PIN setup is shown to Family Link and regular users in tablet
// mode.
IN_PROC_BROWSER_TEST_P(PinForLoginSetupScreenTest, TabletMode) {
  ash::ShellTestApi().SetTabletModeEnabledForTest(true);
  ShowPinSetupScreen();

  WaitForScreenShown();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kBackButton})->Wait();

  EnterPin();
  test::OobeJS().TapOnPath(kNextButton);
  test::OobeJS().CreateVisibilityWaiter(true, {kDoneButton})->Wait();

  test::OobeJS().TapOnPath(kDoneButton);

  WaitForScreenExit();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::DONE);
  histogram_tester_.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Pin-setup.Done", 1);
  histogram_tester_.ExpectTotalCount("OOBE.StepCompletionTime.Pin-setup", 1);
  EXPECT_THAT(
      histogram_tester_.GetAllSamples("OOBE.PinSetupScreen.UserActions"),
      ElementsAre(base::Bucket(
          static_cast<int>(PinSetupScreen::UserAction::kDoneButtonClicked),
          1)));
}

}  // namespace chromeos
