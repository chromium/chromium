// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/password_selection_screen.h"

#include <memory>
#include <optional>
#include <utility>

#include "base/functional/bind.h"
#include "base/location.h"
#include "chrome/browser/ash/login/screens/osauth/cryptohome_recovery_setup_screen.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/policy/profile_policy_connector.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/test/base/fake_gaia_mixin.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "content/public/test/browser_test.h"
#include "password_selection_screen.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kLocalPasswordButton = {"password-selection",
                                           "localPasswordButton"};
const test::UIPath kNextButton = {"password-selection", "nextButton"};

const char kFakeSmartcardLabel[] = "gaia";

AuthFactorsConfiguration GetFakeAuthFactorConfiguration(
    cryptohome::AuthFactorType configured_factor,
    cryptohome::KeyLabel label) {
  cryptohome::AuthFactorsSet factors;
  factors.Put(cryptohome::AuthFactorType::kPassword);
  factors.Put(cryptohome::AuthFactorType::kPin);
  if (!factors.Has(configured_factor)) {
    factors.Put(configured_factor);
  }
  cryptohome::AuthFactorRef ref(configured_factor, label);
  cryptohome::AuthFactor factor(ref, cryptohome::AuthFactorCommonMetadata());
  return AuthFactorsConfiguration{{factor}, factors};
}

}  // namespace

class PasswordSelectionScreenTest : public OobeBaseTest {
 public:
  PasswordSelectionScreenTest() {
  }
  ~PasswordSelectionScreenTest() override = default;

  void SetUpOnMainThread() override {
    fake_gaia_.SetupFakeGaiaForLoginWithDefaults();
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
    result_ = std::nullopt;
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

  void StartLogin() {
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
  }

  void WaitForScreen() {
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

  std::unique_ptr<UserContext> BorrowUserContext() {
    return ash::AuthSessionStorage::Get()->BorrowForTests(
        FROM_HERE, LoginDisplayHost::default_host()
                       ->GetWizardContextForTesting()
                       ->extra_factors_token.value());
  }

  void StoreUserContext(std::unique_ptr<UserContext> context) {
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(context));
  }

  std::optional<PasswordSelectionScreen::Result> result_;

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

  FakeGaiaMixin fake_gaia_{&mixin_host_};

  PasswordSelectionScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;

  CryptohomeRecoverySetupScreen::ScreenExitCallback recovery_original_callback_;
  std::optional<CryptohomeRecoverySetupScreen::Result> recovery_result_;
  base::RepeatingClosure recovery_screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, GaiaPasswordChoice) {
  StartLogin();
  WaitForScreen();
  test::OobeJS().ExpectVisiblePath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_CHOICE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, LocalPasswordChoice) {
  StartLogin();
  WaitForScreen();
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

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, Managed) {
  StartLogin();
  ProfileManager::GetPrimaryUserProfile()
      ->GetProfilePolicyConnector()
      ->OverrideIsManagedForTesting(/*is_managed=*/true);
  WaitForScreen();
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_ENTERPRISE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, SmartCard) {
  StartLogin();
  auto user_context = BorrowUserContext();
  user_context->SetAuthFactorsConfiguration(GetFakeAuthFactorConfiguration(
      cryptohome::AuthFactorType::kSmartCard,
      cryptohome::KeyLabel{kFakeSmartcardLabel}));
  StoreUserContext(std::move(user_context));
  WaitForScreen();
  WaitForScreenExit();
  EXPECT_EQ(result_.value(), PasswordSelectionScreen::Result::NOT_APPLICABLE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, RecoveryLocalPassword) {
  StartLogin();
  auto user_context = BorrowUserContext();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->knowledge_factor_setup.auth_setup_flow =
      WizardContext::AuthChangeFlow::kRecovery;
  user_context->SetAuthFactorsConfiguration(GetFakeAuthFactorConfiguration(
      cryptohome::AuthFactorType::kPassword,
      cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}));
  StoreUserContext(std::move(user_context));
  WaitForScreen();
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::LOCAL_PASSWORD_FORCED);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest, RecoveryGaiaPassword) {
  StartLogin();
  auto user_context = BorrowUserContext();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->knowledge_factor_setup.auth_setup_flow =
      WizardContext::AuthChangeFlow::kRecovery;
  user_context->SetAuthFactorsConfiguration(GetFakeAuthFactorConfiguration(
      cryptohome::AuthFactorType::kPassword,
      cryptohome::KeyLabel{kCryptohomeGaiaKeyLabel}));
  StoreUserContext(std::move(user_context));
  WaitForScreen();
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_FALLBACK);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest,
                       RecoveryWithNoPasswordGAIAChoice) {
  StartLogin();
  auto user_context = BorrowUserContext();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->knowledge_factor_setup.auth_setup_flow =
      WizardContext::AuthChangeFlow::kRecovery;
  StoreUserContext(std::move(user_context));
  WaitForScreen();
  test::OobeJS().ExpectVisiblePath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::GAIA_PASSWORD_CHOICE);
}

IN_PROC_BROWSER_TEST_F(PasswordSelectionScreenTest,
                       RecoveryWithNoPasswordLocalChoice) {
  StartLogin();
  auto user_context = BorrowUserContext();
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->knowledge_factor_setup.auth_setup_flow =
      WizardContext::AuthChangeFlow::kRecovery;
  StoreUserContext(std::move(user_context));
  WaitForScreen();
  test::OobeJS().ExpectVisiblePath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kLocalPasswordButton);
  test::OobeJS().ClickOnPath(kNextButton);
  WaitForScreenExit();
  EXPECT_EQ(result_.value(),
            PasswordSelectionScreen::Result::LOCAL_PASSWORD_CHOICE);
}

}  // namespace ash
