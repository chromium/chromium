// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/recovery_eligibility_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/test/wizard_controller_screen_exit_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/recovery_eligibility_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/policy/core/common/cloud/test/policy_builder.h"
#include "components/policy/proto/cloud_policy.pb.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class RecoveryEligibilityScreenTest : public OobeBaseTest {
 public:
  RecoveryEligibilityScreenTest() {}

  ~RecoveryEligibilityScreenTest() override = default;

  void SetUpOnMainThread() override {
    // Enable recovery factor support in FUDAC
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(true);

    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&RecoveryEligibilityScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    OobeBaseTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    OobeBaseTest::TearDownOnMainThread();
    result_ = std::nullopt;
  }

  virtual void LoginAsUserImpl(bool is_child) = 0;

  void LoginAsUser(bool is_child) {
    // Login, and skip the post login screens.
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;
    context->defer_oobe_flow_finished_for_tests = true;
    LoginAsUserImpl(is_child);
    WizardControllerExitWaiter(UserCreationView::kScreenId).Wait();
    WaitForScreenExit();
  }

  RecoveryEligibilityScreen* GetScreen() {
    return static_cast<RecoveryEligibilityScreen*>(
        WizardController::default_controller()->screen_manager()->GetScreen(
            RecoveryEligibilityView::kScreenId));
  }

  void WaitForScreenExit() {
    if (result_.has_value()) {
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void ContinueScreenExit() {
    original_callback_.Run(result_.value());
  }

  FakeGaiaMixin fake_gaia_{&mixin_host_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_, {}, &fake_gaia_};
  CryptohomeMixin cryptohome_{&mixin_host_};
  std::optional<RecoveryEligibilityScreen::Result> result_;

 protected:
  void HandleScreenExit(RecoveryEligibilityScreen::Result result) {
    result_ = result;
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  base::test::ScopedFeatureList feature_list_;
  RecoveryEligibilityScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

class RecoveryEligibilityScreenConsumerTest
    : public RecoveryEligibilityScreenTest {
 public:
  RecoveryEligibilityScreenConsumerTest() {}
  ~RecoveryEligibilityScreenConsumerTest() override = default;

  void LoginAsUserImpl(bool is_child) override {
    if (is_child) {
      // Child users require user policy. Set up an empty one so the user can
      // get through login.
      ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
      login_manager_mixin_.LoginAsNewChildUser();
    } else {
      login_manager_mixin_.LoginAsNewRegularUser();
    }
  }

 protected:
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(test::kTestEmail, test::kTestGaiaId)};
};

class RecoveryEligibilityScreenEnterpriseTest
    : public RecoveryEligibilityScreenTest {
 public:
  RecoveryEligibilityScreenEnterpriseTest() {}
  ~RecoveryEligibilityScreenEnterpriseTest() override = default;

  void LoginAsUserImpl(bool is_child) override {
    CHECK(!is_child);
    ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
    login_manager_mixin_.LoginAsNewEnterpriseUser();
  }

 protected:
  EmbeddedPolicyTestServerMixin policy_server_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(FakeGaiaMixin::kEnterpriseUser1,
                                     FakeGaiaMixin::kEnterpriseUser1GaiaId),
      &policy_server_};
};

// The recovery fields on the context should be set correctly for unmanaged
// users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenConsumerTest, RegularUser) {
  LoginAsUser(/*is_child=*/false);

  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.ask_about_recovery_consent);
  const bool opt_in = base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryByDefaultForConsumers);
  EXPECT_EQ(opt_in, LoginDisplayHost::default_host()
                        ->GetWizardContextForTesting()
                        ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

// The recovery fields on the context should be set correctly for child users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenConsumerTest, ChildUser) {
  LoginAsUser(/*is_child=*/true);

  EXPECT_TRUE(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->recovery_setup.ask_about_recovery_consent);
  const bool opt_in = base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryByDefaultForConsumers);
  EXPECT_EQ(opt_in, LoginDisplayHost::default_host()
                        ->GetWizardContextForTesting()
                        ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
}

// The recovery fields on the context should be set correctly for managed users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenEnterpriseTest,
                       ManagedUserRecoveryDefault) {
  LoginAsUser(/*is_child=*/false);

  EXPECT_FALSE(LoginDisplayHost::default_host()
                   ->GetWizardContextForTesting()
                   ->recovery_setup.ask_about_recovery_consent);

  const bool enterprise_opt_in = base::FeatureList::IsEnabled(
      ash::features::kCryptohomeRecoveryByDefaultForEnterprise);

  EXPECT_EQ(enterprise_opt_in, LoginDisplayHost::default_host()
                                   ->GetWizardContextForTesting()
                                   ->recovery_setup.recovery_factor_opted_in);

  ContinueScreenExit();
  EXPECT_EQ(result_.value(), RecoveryEligibilityScreen::Result::PROCEED);
}

// The recovery fields on the context should be set correctly for managed users.
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenEnterpriseTest,
                       ManagedUserRecoveryEnabled) {
  user_policy_mixin_.RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_recoveryfactorbehavior()
      ->set_value(true);

  LoginAsUser(/*is_child=*/false);

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
IN_PROC_BROWSER_TEST_F(RecoveryEligibilityScreenEnterpriseTest,
                       ManagedUserRecoveryDisabled) {
  user_policy_mixin_.RequestPolicyUpdate()
      ->policy_payload()
      ->mutable_recoveryfactorbehavior()
      ->set_value(false);

  LoginAsUser(/*is_child=*/false);

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
