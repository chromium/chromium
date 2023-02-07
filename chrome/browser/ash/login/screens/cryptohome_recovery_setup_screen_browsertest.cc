// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/cryptohome_recovery_setup_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class CryptohomeRecoverySetupScreenTest : public OobeBaseTest {
 public:
  CryptohomeRecoverySetupScreenTest() {
    feature_list_.InitAndEnableFeature(features::kCryptohomeRecovery);
  }

  ~CryptohomeRecoverySetupScreenTest() override = default;

  void SetUpOnMainThread() override {
    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &CryptohomeRecoverySetupScreenTest::HandleScreenExit,
        base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    result_ = absl::nullopt;
  }

  void LoginAsRegularUser() {
    // Login, and skip the post login screens.
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;
    context->defer_oobe_flow_finished_for_tests = true;
    login_manager_mixin_.LoginAsNewRegularUser();
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    // Wait for the recovery screen and copy the user context before it is
    // cleared.
    WaitForScreenExit();
    auto user_context =
        std::make_unique<UserContext>(*context->extra_factors_auth_session);
    cryptohome_.MarkUserAsExisting(user_context->GetAccountId());
    ContinueScreenExit();
    // Wait until the OOBE flow finishes before we set new values on the wizard
    // context.
    OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

    // Set the values on the wizard context: the `extra_factors_auth_session`
    // is available after the previous screens have run regularly, and it holds
    // an authenticated auth session.
    user_context->ResetAuthSessionId();
    user_context->SetAuthSessionId(cryptohome_.AddSession(
        user_context->GetAccountId(), /*authenticated=*/true));
    context->extra_factors_auth_session = std::move(user_context);
    context->skip_post_login_screens_for_tests = false;
    // Clear the test state.
    result_ = absl::nullopt;
  }

  CryptohomeRecoverySetupScreen* GetScreen() {
    return WizardController::default_controller()
        ->GetScreen<CryptohomeRecoverySetupScreen>();
  }

  void WaitForScreenExit() {
    if (result_.has_value())
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ContinueScreenExit() {
    original_callback_.Run(result_.value());
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  void ShowScreen() {
    LoginDisplayHost::default_host()->StartWizard(
        CryptohomeRecoverySetupScreenView::kScreenId);
    WaitForScreenExit();
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};
  absl::optional<CryptohomeRecoverySetupScreen::Result> result_;

 private:
  void HandleScreenExit(CryptohomeRecoverySetupScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  CryptohomeRecoverySetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

// If user opts out from recovery, the screen should be skipped.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest, SkippedOnOptOut) {
  LoginAsRegularUser();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.ask_about_recovery_consent = true;
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.recovery_factor_opted_in = false;
  base::HistogramTester histogram_tester;

  ShowScreen();

  ContinueScreenExit();
  EXPECT_EQ(result_.value(),
            CryptohomeRecoverySetupScreen::Result::NOT_APPLICABLE);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Cryptohome-recovery-setup.Done", 0);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTime.Cryptohome-recovery-setup", 0);
}

// If user opts in to recovery, the screen should be shown. In this case
// `extra_factors_auth_session` should not be cleared.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest,
                       ShowDoesntClearAuthSession) {
  LoginAsRegularUser();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.ask_about_recovery_consent = true;
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.recovery_factor_opted_in = true;
  EXPECT_FALSE(PinSetupScreen::ShouldSkipBecauseOfPolicy());
  base::HistogramTester histogram_tester;

  ShowScreen();
  EXPECT_NE(LoginDisplayHost::default_host()
                ->GetWizardContextForTesting()
                ->extra_factors_auth_session,
            nullptr);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoverySetupScreen::Result::DONE);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Cryptohome-recovery-setup.Done", 1);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTime.Cryptohome-recovery-setup", 1);
}

// If user opts in to recovery, the screen should be shown.
// The PIN setup screen is skipped. In this case `extra_factors_auth_session`
// should be cleared.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest,
                       ShowClearsAuthSession) {
  LoginAsRegularUser();
  auto test_api = std::make_unique<quick_unlock::TestApi>(
      /*override_quick_unlock=*/true);
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.ask_about_recovery_consent = true;
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.recovery_factor_opted_in = true;
  EXPECT_TRUE(PinSetupScreen::ShouldSkipBecauseOfPolicy());
  base::HistogramTester histogram_tester;

  ShowScreen();
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContextForTesting()
                ->extra_factors_auth_session,
            nullptr);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoverySetupScreen::Result::DONE);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Cryptohome-recovery-setup.Done", 1);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTime.Cryptohome-recovery-setup", 1);
}

}  // namespace ash
