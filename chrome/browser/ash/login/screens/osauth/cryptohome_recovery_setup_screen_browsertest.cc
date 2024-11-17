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
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/quick_unlock/quick_unlock_utils.h"
#include "chrome/browser/ash/login/screens/sync_consent_screen.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/sync_consent_screen_handler.h"
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

    // SyncConsentScreen is immediately before CryptohomeRecoverySetupScreen. We
    // use this callback to carefully stop the OOBE flow just before it so that
    // we can prepare the opt-in value.
    sync_consent_callback_ =
        GetSyncConsentScreen()->get_exit_callback_for_testing();
    GetSyncConsentScreen()->set_exit_callback_for_testing(
        sync_consent_result_waiter_.GetRepeatingCallback());

    OobeBaseTest::SetUpOnMainThread();
  }

  void SimulateRecoveryFactorOptInValue(bool opted_in) {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->recovery_setup.ask_about_recovery_consent = true;
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->recovery_setup.recovery_factor_opted_in = opted_in;
  }

  void LoginAndWaitForSyncConsentScreenExit() {
    // Login, and skip the post login screens until the SyncConsentScreen.
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;
    login_manager_mixin_.LoginAsNewRegularUser();

    // Wait for the SyncConsentScreen to exit and set the system to not skip any
    // more screens. `CryptohomeRecoverySetup` will be shown next.
    ASSERT_TRUE(sync_consent_result_waiter_.Wait());
    context->skip_post_login_screens_for_tests = false;
  }

  void ProceedAndWaitForScreenToExit() {
    // Trigger the continuation of the SyncConsentScreen which will lead to the
    // CryptohomeRecoverySetupScreen and wait for it to exit.
    sync_consent_callback_.Run(sync_consent_result_waiter_.Take());
    WaitForScreenExit();
  }

  void ExpectScreenExitMetrics(bool opted_in) {
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTimeByExitReason.Cryptohome-recovery-setup.Done",
        opted_in ? 1 : 0);
    histogram_tester_.ExpectTotalCount(
        "OOBE.StepCompletionTime.Cryptohome-recovery-setup", opted_in ? 1 : 0);
  }

  CryptohomeRecoverySetupScreen* GetScreen() {
    return WizardController::default_controller()
        ->GetScreen<CryptohomeRecoverySetupScreen>();
  }

  SyncConsentScreen* GetSyncConsentScreen() {
    return WizardController::default_controller()
        ->GetScreen<SyncConsentScreen>();
  }

  void WaitForScreenExit() {
    if (result_.has_value())
      return;

    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};
  std::optional<CryptohomeRecoverySetupScreen::Result> result_;

 private:
  void HandleScreenExit(CryptohomeRecoverySetupScreen::Result result) {
    result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  base::HistogramTester histogram_tester_;

  CryptohomeRecoverySetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;

  SyncConsentScreen::ScreenExitCallback sync_consent_callback_;
  base::test::TestFuture<SyncConsentScreen::Result> sync_consent_result_waiter_;
};

// If user opts out from recovery, the screen should be skipped.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest, SkippedOnOptOut) {
  LoginAndWaitForSyncConsentScreenExit();

  SimulateRecoveryFactorOptInValue(/*opted_in=*/false);

  ProceedAndWaitForScreenToExit();

  EXPECT_EQ(result_.value(),
            CryptohomeRecoverySetupScreen::Result::NOT_APPLICABLE);
  ExpectScreenExitMetrics(/*opted_in=*/false);
}

// If user opts in to recovery, the screen should be shown. In this case
// auth session should not be cleared.
IN_PROC_BROWSER_TEST_F(CryptohomeRecoverySetupScreenTest,
                       ShowDoesntClearAuthSession) {
  LoginAndWaitForSyncConsentScreenExit();

  SimulateRecoveryFactorOptInValue(/*opted_in=*/true);

  ProceedAndWaitForScreenToExit();

  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->extra_factors_token.has_value());

  EXPECT_EQ(result_.value(), CryptohomeRecoverySetupScreen::Result::DONE);
  ExpectScreenExitMetrics(/*opted_in=*/true);
}

}  // namespace ash
