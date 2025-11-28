// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/test/shell_test_api.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/auth_ui_utils.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/test/user_policy_mixin.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ash/policy/test_support/embedded_policy_test_server_mixin.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/cryptohome_recovery_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/local_password_setup_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/chromeos/devicetype_utils.h"

namespace ash {
namespace {

using ::testing::ElementsAre;

constexpr auto* kPinSetupScreen = PinSetupScreenView::kScreenId.name;
constexpr char kPinSetupScreenCompletionTime[] =
    "OOBE.StepCompletionTime.Pin-setup";
constexpr char kPinSetupScreenCompletionTimeByExitReason[] =
    "OOBE.StepCompletionTimeByExitReason.Pin-setup.";
constexpr char kPinSetupScreenUserAction[] = "OOBE.PinSetupScreen.UserActions";

const test::UIPath kPinSetupScreenDoneStep = {kPinSetupScreen, "doneDialog"};

const test::UIPath kBackButton = {kPinSetupScreen, "backButton"};
const test::UIPath kNextButton = {kPinSetupScreen, "nextButton"};
const test::UIPath kSkipButton = {kPinSetupScreen, "setupSkipButton"};
const test::UIPath kSkipButtonCore = {kPinSetupScreen, "setupSkipButton",
                                      "button"};
const test::UIPath kDoneButton = {kPinSetupScreen, "doneButton"};
const test::UIPath kPinKeyboardInput = {kPinSetupScreen, "pinKeyboard",
                                        "pinKeyboard", "pinInput"};
const test::UIPath kSetupTitle = {kPinSetupScreen, "setupTitle"};
const test::UIPath kSetupSubtitle = {kPinSetupScreen, "setupSubtitle"};

const test::UIPath kPinInputField = {kPinSetupScreen, "pinKeyboard",
                                     "pinKeyboard", "pinInput"};

const test::UIPath kShowHidePinButton = {kPinSetupScreen, "pinKeyboard",
                                         "pinKeyboard", "showPinButton"};

// PasswordSelectionScreen elements.
const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kNextButtonPasswordSelection = {"password-selection",
                                                   "nextButton"};
const test::UIPath kBackButtonPasswordSelection = {"password-selection",
                                                   "backButton"};

PinSetupScreen* GetScreen() {
  return WizardController::default_controller()->GetScreen<PinSetupScreen>();
}

FingerprintSetupScreen* GetFingerprintScreen() {
  return WizardController::default_controller()
      ->GetScreen<FingerprintSetupScreen>();
}

CryptohomeRecoverySetupScreen* GetCryptohomeSetupScreen() {
  return WizardController::default_controller()
      ->GetScreen<CryptohomeRecoverySetupScreen>();
}

void TapSkipButton() {
  test::OobeJS().TapOnPath(kSkipButton);
}

void TapNextButton() {
  test::OobeJS().TapOnPath(kNextButton);
}

void TapDoneButton() {
  test::OobeJS().CreateVisibilityWaiter(true, kPinSetupScreenDoneStep)->Wait();
  test::OobeJS().TapOnPath(kDoneButton);
}

void EnterPin() {
  test::OobeJS().TypeIntoPath("654321", kPinKeyboardInput);
}

void InsertAndConfirmPin() {
  EnterPin();
  TapNextButton();
  // Wait until the back button is visible to ensure that the UI is showing
  // the 'confirmation' step.
  test::OobeJS().CreateVisibilityWaiter(true, kBackButton)->Wait();
  EnterPin();
  TapNextButton();
  TapDoneButton();
}

void HandlePasswordSelectionScreen() {
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(kGaiaPasswordButton);
  test::OobeJS().ClickOnPath(kNextButtonPasswordSelection);
}

void WaitForSetupTitleAndSubtitle(int title_msg_id,
                                  int subtitle_msg_id,
                                  bool subtitle_has_device_name = false) {
  auto expected_title = l10n_util::GetStringUTF8(title_msg_id);
  auto expected_subtitle =
      subtitle_has_device_name
          ? l10n_util::GetStringFUTF8(subtitle_msg_id,
                                      ui::GetChromeOSDeviceName())
          : l10n_util::GetStringUTF8(subtitle_msg_id);

  test::OobeJS()
      .CreateElementTextContentWaiter(expected_title, kSetupTitle)
      ->Wait();
  test::OobeJS()
      .CreateElementTextContentWaiter(expected_subtitle, kSetupSubtitle)
      ->Wait();
}

void ExpectExtraFactorsTokenPresence(bool present) {
  EXPECT_EQ(LoginDisplayHost::default_host()
                ->GetWizardContextForTesting()
                ->extra_factors_token.has_value(),
            present);
}

}  // namespace

// Base class for testing the PIN setup screen. By default, this class simulates
// "hardware support" (a.k.a. login support) for PIN as it is more common across
// the fleet.
class PinSetupScreenTest : public OobeBaseTest {
 public:
  PinSetupScreenTest() {
    UserDataAuthClient::InitializeFake();
    SetHardwareSupport(true);
  }

  ~PinSetupScreenTest() override = default;

  // This must be called very early (e.g. in the constructor) so that the
  // hardware support flag before `PinSetupScreen` reads it.
  static void SetHardwareSupport(bool is_supported) {
    FakeUserDataAuthClient::TestApi::Get()
        ->set_supports_low_entropy_credentials(is_supported);
  }

  static void SetTabletMode(bool in_tablet_mode) {
    ShellTestApi().SetTabletModeEnabledForTest(in_tablet_mode);
  }

  void SetAllowPinUnlockPolicyForEnterpriseUsers() {
    enterprise_management::CloudPolicySettings* policy =
        user_policy_mixin_.RequestPolicyUpdate()->policy_payload();
    policy->mutable_quickunlockmodeallowlist()->mutable_value()->add_entries(
        "PIN");
    policy_server_.UpdateUserPolicy(*policy, FakeGaiaMixin::kEnterpriseUser1);
  }

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    // PinSetupScren exit result manipulation.
    PinSetupScreen::ScreenExitCallback original_callback =
        GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(
        base::BindLambdaForTesting([&, callback = std::move(original_callback)](
                                       PinSetupScreen::Result result) {
          // Save the result and trigger the original callback. This ensures
          // that metrics are properly recorded after the screen exits.
          std::move(screen_exit_result_waiter_.GetRepeatingCallback())
              .Run(result);
          callback.Run(result);
        }));

    // FingerprintSetupScreen exit result manipulation.
    original_fingerprint_callback_ =
        GetFingerprintScreen()->get_exit_callback_for_testing();
    GetFingerprintScreen()->set_exit_callback_for_testing(
        fingerprint_result_waiter_.GetRepeatingCallback());

    // CryptohomeRecoverySetupScreen exit result manipulation.
    cryptohome_recovery_setup_callback_ =
        GetCryptohomeSetupScreen()->get_exit_callback_for_testing();
    GetCryptohomeSetupScreen()->set_exit_callback_for_testing(
        cryptohome_recovery_setup_result_waiter_.GetRepeatingCallback());
  }

  // CryptohomeSetupScreen is the first step into the AuthFactorSetup flow. This
  // will wait until it exits and set the system to not skip any more screens.
  void LoginAndWaitForCryptohomeSetupScreenExit() {
    auto* context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    context->skip_post_login_screens_for_tests = true;

    if (login_as_enterprise_) {
      ASSERT_TRUE(user_policy_mixin_.RequestPolicyUpdate());
      login_manager_mixin_.LoginAsNewEnterpriseUser();
    } else {
      if (simulate_passwordless_signin_) {
        auto user_context = LoginManagerMixin::CreateDefaultUserContext(
            LoginManagerMixin::TestUserInfo(
                AccountId::FromUserEmailGaiaId(test::kTestEmail,
                                               GaiaId(test::kTestGaiaId)),
                /*factors=*/{}));
        login_manager_mixin_.LoginAsNewRegularUser(std::move(user_context));
      } else {
        login_manager_mixin_.LoginAsNewRegularUser();
      }
    }

    ASSERT_TRUE(cryptohome_recovery_setup_result_waiter_.Wait());
    context->skip_post_login_screens_for_tests = false;
  }

  // Unblocks the CryptohomeRecoverySetup screen exit and continues the flow.
  void CryptohomeRecoverySetupContinue() {
    cryptohome_recovery_setup_callback_.Run(
        cryptohome_recovery_setup_result_waiter_.Take());
  }

  // Logs in and moves the flow to the point where the PinSetupScreen would be
  // shown.
  void ShowPinSetupScreen() {
    LoginAndWaitForCryptohomeSetupScreenExit();
    CryptohomeRecoverySetupContinue();
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(PinSetupScreenView::kScreenId).Wait();
  }

  void WaitForScreenExit() { ASSERT_TRUE(screen_exit_result_waiter_.Wait()); }

  void WaitForFingerprintScreenExit() {
    ASSERT_TRUE(fingerprint_result_waiter_.Wait());
  }

  void ExpectFingerprintScreenExitedAndContinue() {
    EXPECT_EQ(fingerprint_result_waiter_.Get(),
              FingerprintSetupScreen::Result::NOT_APPLICABLE);
    original_fingerprint_callback_.Run(
        FingerprintSetupScreen::Result::NOT_APPLICABLE);
  }

  void CheckCredentialsWereCleared() {
    ExpectExtraFactorsTokenPresence(/*present=*/false);
  }

  void CheckCredentialsArePresent() {
    ExpectExtraFactorsTokenPresence(/*present=*/true);
  }

  void ExpectUserActionMetric(PinSetupScreen::UserAction user_action) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kPinSetupScreenUserAction),
        ElementsAre(base::Bucket(static_cast<int>(user_action), /*count=*/1)));
  }

  void ExpectExitResult(PinSetupScreen::Result result) {
    EXPECT_EQ(screen_exit_result_waiter_.Get(), result);
    // Clear the result so that it can be used if the screen is surfaced again.
    screen_exit_result_waiter_.Clear();
  }

  void ExpectExitResultAndMetric(PinSetupScreen::Result result) {
    ExpectExitResult(result);

    if (result == PinSetupScreen::Result::kNotApplicable ||
        result == PinSetupScreen::Result::kNotApplicableAsPrimaryFactor) {
      histogram_tester_.ExpectTotalCount(kPinSetupScreenCompletionTime,
                                         /*expected_count=*/0);
    } else {
      const std::string metric_name =
          kPinSetupScreenCompletionTimeByExitReason +
          PinSetupScreen::GetResultString(result);
      histogram_tester_.ExpectTotalCount(metric_name, 1);
      histogram_tester_.ExpectTotalCount(kPinSetupScreenCompletionTime,
                                         /*expected_count=*/1);
    }
  }

  void ExpectSkipReason(PinSetupScreen::SkipReason reason) {
    ASSERT_TRUE(GetScreen()->get_skip_reason_for_testing().has_value());
    EXPECT_EQ(GetScreen()->get_skip_reason_for_testing().value(), reason);
  }

 protected:
  // Whether to login as a regular user, or as an enterprise user.
  bool login_as_enterprise_ = false;

  // Used for simulating that the user went through Gaia signin using a
  // passwordless method.
  bool simulate_passwordless_signin_ = false;

 private:
  base::test::TestFuture<PinSetupScreen::Result> screen_exit_result_waiter_;

  FingerprintSetupScreen::ScreenExitCallback original_fingerprint_callback_;
  base::test::TestFuture<FingerprintSetupScreen::Result>
      fingerprint_result_waiter_;

  CryptohomeRecoverySetupScreen::ScreenExitCallback
      cryptohome_recovery_setup_callback_;
  base::test::TestFuture<CryptohomeRecoverySetupScreen::Result>
      cryptohome_recovery_setup_result_waiter_;

  // Utilities and Mixins
  base::HistogramTester histogram_tester_;
  EmbeddedPolicyTestServerMixin policy_server_{&mixin_host_};
  UserPolicyMixin user_policy_mixin_{
      &mixin_host_,
      AccountId::FromUserEmailGaiaId(
          FakeGaiaMixin::kEnterpriseUser1,
          GaiaId(FakeGaiaMixin::kEnterpriseUser1GaiaId)),
      &policy_server_};
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};
};

// Tests that the strings are correct when setting up PIN as the main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, TitleAndSubtitleStrings) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  WaitForSetupTitleAndSubtitle(
      IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_TITLE,
      IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_SUBTITLE,
      /*subtitle_has_device_name=*/true);

  // Check that the 'Skip' button shows 'Use password instead'
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_SKIP),
      kSkipButtonCore);
}

// The password selection screen should be shown when the user does not want to
// set up a PIN as a main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest,
                       SkippingLeadsToPasswordSelectionScreen) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();

  // Wait for the password selection screen to be surfaced.
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserChosePassword);
  OobeScreenWaiter(PasswordSelectionScreenHandler::kScreenId).Wait();
}

// When PIN is set as a main factor, the flow continues into the fingerprint
// setup screen, which *always* leads to the PIN setup screen. But when the PIN
// has already been set, the screen is skipped and the auth flow is finished.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, MainFactorSet) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  InsertAndConfirmPin();
  WaitForScreenExit();

  // The flow will exit and continue into the fingerprint setup screen. Ensure
  // that the credentials are still present.
  CheckCredentialsArePresent();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kDoneAsMainFactor);
  ExpectFingerprintScreenExitedAndContinue();

  // Flow must have finished.
  CheckCredentialsWereCleared();
}

// PIN is not offered as a second factor when the user explicitly chooses a
// password.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest,
                       NoAdditionalPinOfferingWhenUserChoosesPassword) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // "Use password instead"
  TapSkipButton();

  // The flow leads to the password selection screen. Ensure that the
  // credentials have not been cleared.
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserChosePassword);
  CheckCredentialsArePresent();
  HandlePasswordSelectionScreen();

  // Once the password is set, the flow continues into fingerprint setup.
  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();

  // PIN is not offered again.
  CheckCredentialsWereCleared();
}

// Ensures that the AuthSession is kept alive when PIN is being offered as the
// main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest,
                       AuthSessionIsKeptAliveForMainFactorSetup) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // Ensure that there is a SessionRefresher keeping the AuthSession alive.
  EXPECT_TRUE(AuthSessionStorage::Get()->CheckHasKeepAliveForTesting(
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->extra_factors_token.value()));
}

// Ensures that the 'eye' icon for showing/hiding the PIN works.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShowHidePin) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  test::OobeJS().CreateVisibilityWaiter(true, kShowHidePinButton)->Wait();

  // Input field should have the 'password' type by default.
  test::OobeJS().ExpectAttributeEQ("type", kPinInputField,
                                   std::string{"password"});
  // Clicking should make the PIN visible.
  test::OobeJS().ClickOnPath(kShowHidePinButton);
  test::OobeJS().ExpectAttributeEQ("type", kPinInputField, std::string{"text"});

  // Back to hidden
  test::OobeJS().ClickOnPath(kShowHidePinButton);
  test::OobeJS().ExpectAttributeEQ("type", kPinInputField,
                                   std::string{"password"});
}

// Tests that the 'Back' button logic on the PasswordSelectionScreen can bring
// the user back to PIN as a main factor setup.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, BackButtonLogicWorks) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // Skip the screen manually.
  TapSkipButton();
  ExpectExitResult(PinSetupScreen::Result::kUserChosePassword);

  // PasswordSelectionScreen should show a back button. Click on it.
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(true, kBackButtonPasswordSelection)
      ->Wait();
  test::OobeJS().ClickOnPath(kBackButtonPasswordSelection);

  // Wait for the PinSetupScreen to be shown again for main factor setup. Check
  // that this is indeed the main factor setup step by verifying the strings and
  // the screen exit result. Wait for PasswordSelection to be shown again.
  WaitForScreenShown();
  WaitForSetupTitleAndSubtitle(
      IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_TITLE,
      IDS_DISCOVER_PIN_SETUP_PIN_AS_MAIN_FACTOR_SUBTITLE,
      /*subtitle_has_device_name=*/true);
  TapSkipButton();
  ExpectExitResult(PinSetupScreen::Result::kUserChosePassword);
  OobeScreenWaiter(PasswordSelectionScreenView::kScreenId).Wait();
}

class PinSetupScreenTestWithoutLoginSupport : public PinSetupScreenTest {
 public:
  PinSetupScreenTestWithoutLoginSupport() { SetHardwareSupport(false); }

  ~PinSetupScreenTestWithoutLoginSupport() override = default;
};

// Tests that the screen is not shown as a main factor when not supported. When
// that is the case, the password selection screen should be shown next.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithoutLoginSupport,
                       NotShownWhenNotSupported) {
  ShowPinSetupScreen();
  WaitForScreenExit();

  // Wait for the password selection screen to be surfaced.
  ExpectSkipReason(PinSetupScreen::SkipReason::kUsupportedHardware);
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);
  OobeScreenWaiter(PasswordSelectionScreenHandler::kScreenId).Wait();
  CheckCredentialsArePresent();
}

class PinSetupScreenTestEnterprise : public PinSetupScreenTest {
 public:
  PinSetupScreenTestEnterprise() { login_as_enterprise_ = true; }
  ~PinSetupScreenTestEnterprise() override = default;

  // Set PINs as allowed for unlock.
  void SetUpOnMainThread() override {
    PinSetupScreenTest::SetUpOnMainThread();
    SetAllowPinUnlockPolicyForEnterpriseUsers();
  }
};

// Tests that the screen is not shown as a main factor for enterprise users even
// when PIN is allowed by policy. It is only offered as a secondary factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestEnterprise,
                       SkippedForEnterpriseUsers) {
  LoginAndWaitForCryptohomeSetupScreenExit();
  CryptohomeRecoverySetupContinue();

  // PIN will not be offered as a main factor for enterprise users.
  WaitForScreenExit();
  ExpectSkipReason(
      PinSetupScreen::SkipReason::kNotSupportedAsPrimaryFactorForManagedUsers);
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);

  // No password selection screen for enterprise users. They must use their
  // online password which is set by `apply-online-password`.
  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();

  // PIN should be offered as a secondary factor instead.
  WaitForScreenShown();
  TapSkipButton();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
  CheckCredentialsWereCleared();
}

class PinSetupScreenTestPasswordlessSignin : public PinSetupScreenTest {
 public:
  PinSetupScreenTestPasswordlessSignin() {
    simulate_passwordless_signin_ = true;
  }
  ~PinSetupScreenTestPasswordlessSignin() override = default;
};

// Tests that the 'Back' button logic on the LocalPasswordSetupScreen can bring
// the user back to PIN as a main factor setup when the user did not have an
// opportunity to choose between an online vs. local password. This is the case
// when the user goes through Gaia using a passwordless method.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestPasswordlessSignin,
                       BackButtonLogicWorks) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // Skip the screen manually.
  TapSkipButton();
  ExpectExitResult(PinSetupScreen::Result::kUserChosePassword);

  // The user did not go through Gaia using a password, thus they do not have
  // the option to choose between an online vs. local password and land on the
  // LocalPasswordSetupScreen directly.
  OobeScreenWaiter(LocalPasswordSetupView::kScreenId).Wait();
  test::OobeJS()
      .CreateVisibilityWaiter(/*visibility=*/true,
                              test::LocalPasswordSetupBackActionPath())
      ->Wait();

  // Clicking on 'Back' should bring us back to main factor PIN setup.
  test::LocalPasswordSetupBackAction();
  WaitForScreenShown();

  // Skip again and verify the proper exit result and transition.
  TapSkipButton();
  ExpectExitResult(PinSetupScreen::Result::kUserChosePassword);
  OobeScreenWaiter(LocalPasswordSetupView::kScreenId).Wait();
}

class PinSetupScreenTestWithoutLoginSupportPasswordlessSignin
    : public PinSetupScreenTestWithoutLoginSupport {
 public:
  PinSetupScreenTestWithoutLoginSupportPasswordlessSignin() {
    simulate_passwordless_signin_ = true;
  }

  ~PinSetupScreenTestWithoutLoginSupportPasswordlessSignin() override = default;
};

// Without hardware support, the PIN screen is not shown for setting up a PIN
// as the main factor. Additionally, when the user goes through Gaia without
// using a password, they land directly on the LocalPasswordSetupScreen. In that
// case, there isn't a back button.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithoutLoginSupportPasswordlessSignin,
                       NoBackButtonOnLocalPasswordSetup) {
  ShowPinSetupScreen();
  WaitForScreenExit();

  // Wait for the password selection screen to be surfaced.
  ExpectSkipReason(PinSetupScreen::SkipReason::kUsupportedHardware);
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);
  OobeScreenWaiter(LocalPasswordSetupView::kScreenId).Wait();

  // Expect the back button to be hidden.
  test::OobeJS().ExpectHiddenPath(test::LocalPasswordSetupBackActionPath());
}

}  // namespace ash
