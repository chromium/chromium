// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/remove_local_auth_factors_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/remove_local_auth_factors_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "content/public/test/browser_test.h"

namespace ash {

class RemoveLocalAuthFactorsScreenTest : public OobeBaseTest {
 public:
  RemoveLocalAuthFactorsScreenTest() {
    feature_list_.InitAndEnableFeature(features::kManagedLocalPinAndPassword);
  }
  ~RemoveLocalAuthFactorsScreenTest() override = default;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    OobeBaseTest::SetUpCommandLine(command_line);
    command_line->AppendSwitch(switches::kAllowFailedPolicyFetchForTest);
  }

  void LoginAndShowRemoveLocalAuthFactorsScreen() {
    // TODO: b/445628245 - Remove the recovery setup interception once we
    // integrate the screen in the actual flow.

    // Setup to intercept CryptohomeRecoverySetupScreen exit
    CryptohomeRecoverySetupScreen* recovery_screen =
        WizardController::default_controller()
            ->GetScreen<CryptohomeRecoverySetupScreen>();
    recovery_original_callback_ =
        recovery_screen->get_exit_callback_for_testing();
    recovery_screen->set_exit_callback_for_testing(
        recovery_setup_result_test_future_.GetRepeatingCallback());

    // Login as a new enterprise user
    login_manager_mixin_.LoginAsNewEnterpriseUser();

    // Wait for the CryptohomeRecoverySetupScreen to be shown and about to exit.
    ASSERT_TRUE(recovery_setup_result_test_future_.Wait());

    WizardController::default_controller()->AdvanceToScreen(
        RemoveLocalAuthFactorsScreenView::kScreenId);
  }

 protected:
  LoginManagerMixin login_manager_mixin_{&mixin_host_};

 private:
  base::test::ScopedFeatureList feature_list_;
  CryptohomeRecoverySetupScreen::ScreenExitCallback recovery_original_callback_;
  base::test::TestFuture<CryptohomeRecoverySetupScreen::Result>
      recovery_setup_result_test_future_;
};

// Test that the screen is shown and the Done button is visible and enabled.
IN_PROC_BROWSER_TEST_F(RemoveLocalAuthFactorsScreenTest,
                       ScreenShownWithDoneButton) {
  LoginAndShowRemoveLocalAuthFactorsScreen();

  OobeScreenWaiter(RemoveLocalAuthFactorsScreenView::kScreenId).Wait();

  const test::UIPath kDoneButtonPath = {"remove-local-auth-factors",
                                        "doneButton"};
  test::OobeJS().CreateVisibilityWaiter(true, kDoneButtonPath)->Wait();
  test::OobeJS().ExpectEnabledPath(kDoneButtonPath);

  // TODO: b/445628245 - Add test for clicking the done button and checking
  // the screen exit result once the exit logic is implemented.
}

}  // namespace ash
