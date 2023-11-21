// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/password_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_setup_screen.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/session_manager_state_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kLocalPasswordButton = {"password-selection",
                                           "localPasswordButton"};
const test::UIPath kNextButton = {"password-selection", "nextButton"};

}  // namespace

class PasswordSelectionScreenTest : public OobeBaseTest {
 public:
  PasswordSelectionScreenTest() {
    feature_list_.InitAndEnableFeature(features::kLocalPasswordForConsumers);
  }
  ~PasswordSelectionScreenTest() override = default;

  void SetUpOnMainThread() override {
    recovery_original_callback_ =
        GetRecoveryScreen()->get_exit_callback_for_testing();
    GetRecoveryScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &PasswordSelectionScreenTest::HandleRecoveryScreenExit,
        base::Unretained(this)));

    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&PasswordSelectionScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    result_ = absl::nullopt;
  }

  CryptohomeRecoverySetupScreen* GetRecoveryScreen() {
    return static_cast<CryptohomeRecoverySetupScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            CryptohomeRecoverySetupScreenView::kScreenId));
  }

  PasswordSelectionScreen* GetScreen() {
    return static_cast<PasswordSelectionScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            PasswordSelectionScreenView::kScreenId));
  }

  void StartLoginAndWaitForScreen() {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->skip_post_login_screens_for_tests = true;
    LoginDisplayHost::default_host()
        ->GetOobeUI()
        ->GetView<GaiaScreenHandler>()
        ->ShowSigninScreenForTest(FakeGaiaMixin::kFakeUserEmail,
                                  FakeGaiaMixin::kFakeUserPassword,
                                  FakeGaiaMixin::kEmptyUserServices);
    // Wait until the previous screen (`CryptohomeRecoverySetupScreen`) and set
    // `skip_post_login_screens_for_tests` to `false` before proceeding.
    // This allows to skip all the screens before the `PasswordSelectionScreen`.
    if (!recovery_result_.has_value()) {
      base::RunLoop run_loop;
      recovery_screen_exit_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->skip_post_login_screens_for_tests = false;
    recovery_original_callback_.Run(recovery_result_.value());
    OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();

    original_callback_.Run(result_.value());
  }

  absl::optional<PasswordSelectionScreen::Result> result_;

 private:
  void HandleScreenExit(PasswordSelectionScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  void HandleRecoveryScreenExit(CryptohomeRecoverySetupScreen::Result result) {
    recovery_result_ = result;
    if (recovery_screen_exit_callback_) {
      std::move(recovery_screen_exit_callback_).Run();
    }
  }

  base::test::ScopedFeatureList feature_list_;
  FakeGaiaMixin fake_gaia_{&mixin_host_};

  PasswordSelectionScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;

  CryptohomeRecoverySetupScreen::ScreenExitCallback recovery_original_callback_;
  absl::optional<CryptohomeRecoverySetupScreen::Result> recovery_result_;
  base::RepeatingClosure recovery_screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, GaiaPasswordChoice) {
  StartLoginAndWaitForScreen();
  test::OobeJS().ExpectVisiblePath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_CHOICE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, LocalPasswordChoice) {
  StartLoginAndWaitForScreen();
  test::OobeJS().ExpectVisiblePath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::LOCAL_PASSWORD_CHOICE);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->knowledge_factor_setup.local_password_forced);
}

}  // namespace ash
