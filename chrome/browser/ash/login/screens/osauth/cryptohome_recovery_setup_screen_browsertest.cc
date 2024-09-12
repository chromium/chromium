// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_setup_screen.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class CryptohomeRecoverySetupScreenTest : public OobeBaseTest {
 public:
  CryptohomeRecoverySetupScreenTest() {}

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
    result_ = std::nullopt;
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
    std::unique_ptr<UserContext> user_context =
        ash::AuthSessionStorage::Get()->BorrowForTests(
            FROM_HERE, context->extra_factors_token.value());
    context->extra_factors_token = std::nullopt;
    cryptohome_.MarkUserAsExisting(user_context->GetAccountId());
    ContinueScreenExit();
    // Wait until the OOBE flow finishes before we set new values on the wizard
    // context.
    OobeScreenExitWaiter(UserCreationView::kScreenId).Wait();

    // Set the values on the wizard context: the `extra_factors_token`
    // is available after the previous screens have run regularly, and it holds
    // an authenticated auth session.
    user_context->ResetAuthSessionIds();
    auto session_ids = cryptohome_.AddSession(user_context->GetAccountId(),
                                              /*authenticated=*/true);
    user_context->SetAuthSessionIds(session_ids.first, session_ids.second);
    user_context->SetSessionLifetime(base::Time::Now() +
                                     cryptohome::kAuthsessionInitialLifetime);
    context->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(user_context));
    context->skip_post_login_screens_for_tests = false;
    // Clear the test state.
    result_ = std::nullopt;
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
  std::optional<CryptohomeRecoverySetupScreen::Result> result_;

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
// auth session should not be cleared.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest,
                       ShowDoesntClearAuthSession) {
  LoginAsRegularUser();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.ask_about_recovery_consent = true;
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->recovery_setup.recovery_factor_opted_in = true;
  base::HistogramTester histogram_tester;

  ShowScreen();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->extra_factors_token.has_value());

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), CryptohomeRecoverySetupScreen::Result::DONE);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTimeByExitReason.Cryptohome-recovery-setup.Done", 1);
  histogram_tester.ExpectTotalCount(
      "OOBE.StepCompletionTime.Cryptohome-recovery-setup", 1);
}

}  // namespace ash
