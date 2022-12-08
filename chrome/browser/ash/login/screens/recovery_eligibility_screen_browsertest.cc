// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/recovery_eligibility_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/run_loop.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class RecoveryEligibilityScreenTest : public OobeBaseTest {
 public:
  RecoveryEligibilityScreenTest() {
    feature_list_.InitAndEnableFeature(features::kCryptohomeRecoverySetup);
  }

  ~RecoveryEligibilityScreenTest() override = default;

  void SetUpOnMainThread() override {
    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&RecoveryEligibilityScreenTest::HandleScreenExit,
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
    result_ = absl::nullopt;
  }

  RecoveryEligibilityScreen* GetScreen() {
    return static_cast<RecoveryEligibilityScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            RecoveryEligibilityView::kScreenId));
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
        RecoveryEligibilityView::kScreenId);
    WaitForScreenExit();
  }

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};
  absl::optional<RecoveryEligibilityScreen::Result> result_;

 private:
  void HandleScreenExit(RecoveryEligibilityScreen::Result result) {
    result_ = result;
  }

  base::test::ScopedFeatureList feature_list_;
  RecoveryEligibilityScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

// The recovery fields on the context should be set correctly for unmanaged
// users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest, UnmanagedUser) {
  LoginAsRegularUser();

  ShowScreen();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.ask_about_recovery_consent);
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

// The recovery fields on the context should be set correctly for managed users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest,
                       ManagedUserRecoveryEnabled) {
  LoginAsRegularUser();
  ProfileManager::GetActiveUserProfile()
      ->GetProfilePolicyConnector()
      ->OverrideIsManagedForTesting(/*is_managed=*/true);
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kRecoveryFactorBehavior, true);

  ShowScreen();
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.ask_about_recovery_consent);
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

// The recovery fields on the context should be set correctly for managed users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest,
                       ManagedUserRecoveryDisabled) {
  LoginAsRegularUser();
  ProfileManager::GetActiveUserProfile()
      ->GetProfilePolicyConnector()
      ->OverrideIsManagedForTesting(/*is_managed=*/true);
  ProfileManager::GetActiveUserProfile()->GetPrefs()->SetBoolean(
      ash::prefs::kRecoveryFactorBehavior, false);

  ShowScreen();
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.ask_about_recovery_consent);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

}  // namespace ash
