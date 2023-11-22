// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/recovery_eligibility_screen.h"

#include <string>
#include <utility>

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
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {

class RecoveryEligibilityScreenTest : public OobeBaseTest {
 public:
  RecoveryEligibilityScreenTest() {}

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

  void LoginAsUser(bool is_child) {
    // Login, and skip the post login screens.
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;
    context->defer_oobe_flow_finished_for_tests = true;
    if (is_child) {
      // Child users require user policy. Set up an empty one so the user can
      // get through login.
      ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
      login_manager_mixin_.LoginAsNewChildUser();
    } else {
      login_manager_mixin_.LoginAsNewRegularUser();
    }
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    WaitForScreenExit();

    std::unique_ptr<UserContext> user_context;
    user_context = ash::AuthSessionStorage::Get()->BorrowForTests(
        FROM_HERE, context->extra_factors_token.value());
    context->extra_factors_token = absl::nullopt;
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
  }

  void ShowScreen() {
    LoginDisplayHost::default_host()->StartWizard(
        RecoveryEligibilityView::kScreenId);
    WaitForScreenExit();
  }

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
  CryptohomeMixin cryptohome_{&mixin_host_};
  absl::optional<RecoveryEligibilityScreen::Result> result_;

 private:
  void HandleScreenExit(RecoveryEligibilityScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  RecoveryEligibilityScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

// The recovery fields on the context should be set correctly for unmanaged
// users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest, UnmanagedUser) {
  LoginAsUser(/*is_child=*/false);

  ShowScreen();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.ask_about_recovery_consent);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

// The recovery fields on the context should be set correctly for managed users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest,
                       ManagedUserRecoveryEnabled) {
  LoginAsUser(/*is_child=*/false);
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
  LoginAsUser(/*is_child=*/false);
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

// The recovery fields on the context should be set correctly for child users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenTest, ChildUser) {
  LoginAsUser(/*is_child=*/true);

  ShowScreen();
  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.ask_about_recovery_consent);
  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
}
}  // namespace ash
