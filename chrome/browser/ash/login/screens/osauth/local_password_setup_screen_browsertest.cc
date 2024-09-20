// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/osauth/local_password_setup_screen.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chromeos/ash/components/cryptohome/auth_factor.h"
#include "chromeos/ash/components/cryptohome/common_types.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/auth_factors_configuration.h"
#include "chromeos/ash/components/login/auth/public/cryptohome_key_constants.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "content/public/test/browser_test.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

constexpr char kLocalPasswordSetupScreen[] = "local-password-setup";

constexpr char kFirstPassword[] = "12345678";
constexpr char kSecondPassword[] = "12345679";
constexpr char kShortPassword[] = "1234567";

const test::UIPath kLocalPasswordSetupScreenFirstInput = {
    kLocalPasswordSetupScreen, "passwordInput", "firstInput"};
const test::UIPath kLocalPasswordSetupScreenConfirmInput = {
    kLocalPasswordSetupScreen, "passwordInput", "confirmInput"};

const test::UIPath kNextButton = {kLocalPasswordSetupScreen, "nextButton"};
const test::UIPath kBackButton = {kLocalPasswordSetupScreen, "backButton"};

}  // namespace

// Fixture to prepare oobe and the local password setup screen.
class LocalPasswordSetupScreenTest : public OobeBaseTest {
 public:
  LocalPasswordSetupScreenTest() { UserDataAuthClient::InitializeFake(); }

  ~LocalPasswordSetupScreenTest() override = default;

  virtual std::unique_ptr<UserContext> CreateUserContext() {
    auto context = std::make_unique<UserContext>();
    context->SetAuthFactorsConfiguration(AuthFactorsConfiguration{});
    return context;
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&LocalPasswordSetupScreenTest::HandleScreenExit,
                            base::Unretained(this)));

    CreateUserContext();

    // Add an authenticated session to the user context used during OOBE. In
    // production, this is set by earlier screens which are skipped in this
    // test.
    std::unique_ptr<UserContext> context = CreateUserContext();
    account_id_ = context->GetAccountId();
    cryptohome_.MarkUserAsExisting(account_id_);
    auto session_ids =
        cryptohome_.AddSession(account_id_, /*authenticated=*/true);
    context->SetAuthSessionIds(session_ids.first, session_ids.second);
    context->SetSessionLifetime(base::Time::Now() +
                                cryptohome::kAuthsessionInitialLifetime);

    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(context));
  }

  LocalPasswordSetupScreen* GetScreen() {
    return WizardController::default_controller()
        ->GetScreen<LocalPasswordSetupScreen>();
  }

  void ShowLocalPasswordSetupScreen() {
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          LocalPasswordSetupView::kScreenId);
    }
  }

  void WaitForLocalPasswordSetupScreenShown() {
    OobeScreenWaiter(LocalPasswordSetupView::kScreenId).Wait();
  }

  void WaitForPasswordSelectionScreenShown() {
    OobeScreenWaiter(LocalPasswordSetupView::kScreenId).Wait();
  }

  void WaitForScreenExit() {
    if (screen_exited_) {
      return;
    }
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  // Checks that the screen has successfully exited, meaning we've successfully
  // set up the local password. `AshAuthFactor::kLocalPassword` should also be
  // present in the modified factors set, indicating that we got no errors
  // from `PasswordFactorEditor`.
  void RunPostScreenExitChecks() {
    EXPECT_EQ(screen_exited_, true);
    EXPECT_EQ(screen_result_, LocalPasswordSetupScreen::Result::kDone);

    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    EXPECT_TRUE(wizard_context->knowledge_factor_setup.modified_factors.Has(
        AshAuthFactor::kLocalPassword));
  }

  void RunScreenNotExitedChecks() {
    EXPECT_EQ(screen_exited_, false);
    EXPECT_FALSE(screen_result_.has_value());

    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    EXPECT_FALSE(wizard_context->knowledge_factor_setup.modified_factors.Has(
        AshAuthFactor::kLocalPassword));
  }

  std::optional<LocalPasswordSetupScreen::Result> screen_result_;
  bool screen_exited_ = false;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};

 protected:
  AccountId account_id_;

 private:
  void HandleScreenExit(LocalPasswordSetupScreen::Result result) {
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_) {
      std::move(screen_exit_callback_).Run();
    }
  }

  LocalPasswordSetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;
};

IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       EmptyFieldsDisablesNextButton) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  // Assert that the input fields are empty.
  test::OobeJS().ExpectElementValue({}, kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().ExpectElementValue({}, kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectDisabledPath(kNextButton);

  test::OobeJS().ClickOnPath(kNextButton);

  base::RunLoop().RunUntilIdle();

  RunScreenNotExitedChecks();
}

// Mismatched first input and confirm input fields should disable the next
// button.
IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       MismatchedFieldsDisablesNextButton) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kSecondPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectDisabledPath(kNextButton);
}

// Matching first input and confirm input fields should enable the next button.
// TODO(crbug.com/329339200): This test is flaky.
IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       DISABLED_MatchingFieldsEnablesNextButton) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectEnabledPath(kNextButton);
}

// TODO(crbug.com/329339200): This test is flaky.
// Matching first input and confirm input fields should enable the next button,
// Subsequent mismatched fields should disable to next button.
IN_PROC_BROWSER_TEST_F(
    LocalPasswordSetupScreenTest,
    DISABLED_MatchedThenUnamtchedEnablesThenDisablesNextButton) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectEnabledPath(kNextButton);

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kSecondPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectDisabledPath(kNextButton);
}

// In addition to the passwords matching, the password has to be of sufficient
// complexity (currently defined as 8 or more characters, subject to change,
// check `ash::auth::CheckLocalPasswordComplexity`).
IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       PasswordTooShortDisablesNextButton) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kShortPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kShortPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectDisabledPath(kNextButton);
}

// Submit the password in onboarding mode. Local password setup screen
// should exit with proper exit code and `kLocalPassword` should be in the
// modified factors set.
// TODO(crbug.com/329339200): This test is flaky.
IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       DISABLED_SetLocalPassword) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectEnabledPath(kNextButton);

  test::OobeJS().ClickOnPath(kNextButton);

  WaitForScreenExit();
  RunPostScreenExitChecks();
}

IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenTest,
                       BackLeadsToPasswordSelectionScreen) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().ExpectEnabledPath(kBackButton);

  test::OobeJS().ClickOnPath(kBackButton);

  WaitForScreenExit();
  WaitForPasswordSelectionScreenShown();
}

// Fixture to set up oobe and initialize the local password setup screen in
// recovery mode. Additionally sets up a dummy local password auth factor for
// our user.
class LocalPasswordSetupScreenRecoveryTest
    : public LocalPasswordSetupScreenTest {
 public:
  LocalPasswordSetupScreenRecoveryTest() = default;
  ~LocalPasswordSetupScreenRecoveryTest() override = default;

  // Create a user context with properly set up `AuthFactorsConfiguration`.
  // Otherwise, `PasswordFactorEditor` will ignore our request due to having no
  // known local password factor set up.
  std::unique_ptr<UserContext> CreateUserContext() override {
    std::unique_ptr<UserContext> context = std::make_unique<UserContext>();

    cryptohome::AuthFactorRef ref{
        cryptohome::AuthFactorType::kPassword,
        cryptohome::KeyLabel{kCryptohomeLocalPasswordKeyLabel}};

    std::vector<cryptohome::AuthFactor> configured_factors{
        cryptohome::AuthFactor{cryptohome::AuthFactor(
            ref, cryptohome::AuthFactorCommonMetadata{})}};

    context->SetAuthFactorsConfiguration(AuthFactorsConfiguration{
        std::move(configured_factors), cryptohome::AuthFactorsSet::All()});

    return context;
  }

  void SetUpOnMainThread() override {
    LocalPasswordSetupScreenTest::SetUpOnMainThread();

    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    wizard_context->knowledge_factor_setup.auth_setup_flow =
        WizardContext::AuthChangeFlow::kRecovery;

    // Add local password key to `FakeUserDataAuthClient`. This is necessary
    // otherwise the `UpdateAuthFactor` call will fail.
    cryptohome_.AddLocalPassword(account_id_, kFirstPassword);
  }
};

// Submit the password in recovery mode. Local password setup screen
// should exit with proper exit code and `kLocalPassword` should be in the
// modified factors set.
// TODO(crbug.com/329339200): This test is flaky.
IN_PROC_BROWSER_TEST_F(LocalPasswordSetupScreenRecoveryTest,
                       DISABLED_UpdateLocalPassword) {
  ShowLocalPasswordSetupScreen();
  WaitForLocalPasswordSetupScreenShown();

  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenFirstInput);
  test::OobeJS().TypeIntoPath(kFirstPassword,
                              kLocalPasswordSetupScreenConfirmInput);

  test::OobeJS().ExpectEnabledPath(kNextButton);

  test::OobeJS().ClickOnPath(kNextButton);

  WaitForScreenExit();
  RunPostScreenExitChecks();
}

}  // namespace ash
