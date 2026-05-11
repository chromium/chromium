// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/pin_setup_screen.h"

#include <optional>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
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
#include "chrome/browser/profiles/profile.h"
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
#include "chromeos/ash/services/auth_factor_config/public/mojom/auth_factor_config.mojom.h"
#include "components/prefs/pref_service.h"
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

const test::UIPath kProblemDiv = {kPinSetupScreen, "pinKeyboard", "problemDiv"};

// PasswordSelectionScreen elements.
const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kNextButtonPasswordSelection = {"password-selection",
                                                   "nextButton"};
const test::UIPath kBackButtonPasswordSelection = {"password-selection",
                                                   "backButton"};

// The default minimum length for the legacy QuickUnlock PIN check is 6 digits.
const char kWeakPin[] = "111111";
const char kStrongPin[] = "978213587623";
const char kExpectedHighComplexityError[] =
    "Must be at least 8 digits and can't contain repeating or ordered "
    "sequences";
const char kExpectedLegacyWeakPinWarning[] = "PIN may be easy to guess";

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

void EnterPin(const std::string& pin) {
  test::OobeJS().TypeIntoPath(pin, kPinKeyboardInput);
}

void InsertAndConfirmPin() {
  EnterPin("654321");
  TapNextButton();
  // Wait until the back button is visible to ensure that the UI is showing
  // the 'confirmation' step.
  test::OobeJS().CreateVisibilityWaiter(true, kBackButton)->Wait();
  EnterPin("654321");
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

void WaitUntilNextButtonEnabled(bool enabled) {
  test::OobeJS().CreateEnabledWaiter(enabled, kNextButton)->Wait();
}

void WaitUntilConfirmationStep() {
  test::OobeJS().CreateVisibilityWaiter(true, kBackButton)->Wait();
}

void ExpectProblemMessage(bool is_error, const std::string& expected_message) {
  // Wait for the 'invisible' attribute to be removed.
  test::OobeJS()
      .CreateAttributePresenceWaiter("invisible", /*presence=*/false,
                                     kProblemDiv)
      ->Wait();

  // Assert the style (error vs. warning).
  test::OobeJS().ExpectHasClass(is_error ? "error" : "warning", kProblemDiv);

  // Assert the actual string.
  test::OobeJS().ExpectElementContainsText(expected_message, kProblemDiv);
}

void ExpectNoProblemMessage() {
  test::OobeJS()
      .CreateAttributePresenceWaiter("invisible", /*presence=*/true,
                                     kProblemDiv)
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

  void SetPinAsAllowedLocalAuthFactorForEnterpriseUsers() {
    enterprise_management::CloudPolicySettings* policy =
        user_policy_mixin_.RequestPolicyUpdate()->policy_payload();
    policy->mutable_subproto1()
        ->mutable_allowedlocalauthfactors()
        ->mutable_value()
        ->add_entries("PIN");
    policy_server_.UpdateUserPolicy(*policy, FakeGaiaMixin::kEnterpriseUser1);
  }

  void SetPinAndLocalPasswordAsAllowedAuthFactorsForEnterpriseUsers() {
    enterprise_management::CloudPolicySettings* policy =
        user_policy_mixin_.RequestPolicyUpdate()->policy_payload();
    auto* allowed_local_auth_factors = policy->mutable_subproto1()
                                           ->mutable_allowedlocalauthfactors()
                                           ->mutable_value();
    allowed_local_auth_factors->add_entries("PIN");
    allowed_local_auth_factors->add_entries("LOCAL_PASSWORD");
    policy_server_.UpdateUserPolicy(*policy, FakeGaiaMixin::kEnterpriseUser1);
  }

  void ClearQuickUnlockModeAllowListForEnterpriseUsers() {
    enterprise_management::CloudPolicySettings* policy =
        user_policy_mixin_.RequestPolicyUpdate()->policy_payload();
    policy->mutable_quickunlockmodeallowlist()
        ->mutable_value()
        ->clear_entries();
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

  void SetSAMLAuthFlow() {
    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    auto user_context = ash::AuthSessionStorage::Get()->BorrowForTests(
        FROM_HERE, *wizard_context->extra_factors_token);
    user_context->SetAuthFlow(UserContext::AUTH_FLOW_GAIA_WITH_SAML);
    wizard_context->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(user_context));
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
  PinSetupScreenTestEnterprise(
      const std::vector<base::test::FeatureRef>& enabled_features,
      const std::vector<base::test::FeatureRef>& disabled_features)
      : scoped_feature_list_() {
    login_as_enterprise_ = true;
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
  }
  PinSetupScreenTestEnterprise()
      : PinSetupScreenTestEnterprise(
            /*enabled_features*/ {},
            /*disabled_features*/ {features::kManagedLocalPinAndPassword}) {}
  ~PinSetupScreenTestEnterprise() override = default;

  // Set PINs as allowed for unlock.
  void SetUpOnMainThread() override {
    PinSetupScreenTest::SetUpOnMainThread();
    SetAllowPinUnlockPolicyForEnterpriseUsers();
  }

 protected:
  base::test::ScopedFeatureList scoped_feature_list_;
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

class PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled
    : public PinSetupScreenTestEnterprise {
 public:
  PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled()
      : PinSetupScreenTestEnterprise(
            /*enabled_features*/ {features::kManagedLocalPinAndPassword},
            /*disabled_features*/ {}) {}

  ~PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled() override = default;
};

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
                       AllowedForEnterpriseUsersAsMainFactor) {
  SetPinAsAllowedLocalAuthFactorForEnterpriseUsers();
  ShowPinSetupScreen();
  WaitForScreenShown();

  InsertAndConfirmPin();
  WaitForScreenExit();

  CheckCredentialsArePresent();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kDoneAsMainFactor);
  ExpectFingerprintScreenExitedAndContinue();

  CheckCredentialsWereCleared();
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
                       NotAllowedForEnterpriseUsersAsMainFactor) {
  LoginAndWaitForCryptohomeSetupScreenExit();
  CryptohomeRecoverySetupContinue();

  // PIN will not be offered as a main factor when not allowed by local auth
  // factors policy.
  WaitForScreenExit();
  ExpectSkipReason(
      PinSetupScreen::SkipReason::kNotAllowedByPolicyAsPrimaryFactor);
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);

  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();

  // PIN should be offered as a secondary factor instead.
  WaitForScreenShown();
  TapSkipButton();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
  CheckCredentialsWereCleared();
}

IN_PROC_BROWSER_TEST_F(
    PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
    NotAllowedForEnterpriseUsersAsMainFactorOrSecondaryFactor) {
  ClearQuickUnlockModeAllowListForEnterpriseUsers();
  LoginAndWaitForCryptohomeSetupScreenExit();
  CryptohomeRecoverySetupContinue();

  // PIN will not be offered as a main factor when not allowed by local auth
  // factors policy.
  WaitForScreenExit();
  ExpectSkipReason(
      PinSetupScreen::SkipReason::kNotAllowedByPolicyAsPrimaryFactor);
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);

  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();

  // PIN will also not be offered a secondary factory.
  WaitForScreenExit();
  ExpectSkipReason(PinSetupScreen::SkipReason::kNotAllowedByPolicy);
  CheckCredentialsWereCleared();
}

IN_PROC_BROWSER_TEST_F(
    PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
    ShowsPasswordSelectionOnEnterpriseUserSkipWithLocalPasswordAndPinFactorsSet) {
  SetPinAndLocalPasswordAsAllowedAuthFactorsForEnterpriseUsers();
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserChosePassword);

  OobeScreenWaiter(PasswordSelectionScreenHandler::kScreenId).Wait();
}

IN_PROC_BROWSER_TEST_F(
    PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
    AppliesOnlinePasswordOnEnterpriseUserSkipWithPinFactorsSet) {
  SetPinAsAllowedLocalAuthFactorForEnterpriseUsers();
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserChosePassword);

  // Password Selection screen is not shown when only PIN is allowed, rather
  // `apply-online-password` screen is shown after which the `fingerprint-setup`
  // screen is shown and the credentials are cleared.
  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();
  CheckCredentialsWereCleared();
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
                       SAMLSkipButtonHiddenWhenLocalPasswordNotAllowed) {
  // Policy allows PIN but NOT Local Password.
  SetPinAsAllowedLocalAuthFactorForEnterpriseUsers();

  LoginAndWaitForCryptohomeSetupScreenExit();
  SetSAMLAuthFlow();
  CryptohomeRecoverySetupContinue();
  WaitForScreenShown();

  // Skip button should be disabled for SAML users when local password is NOT
  // allowed.
  test::OobeJS().ExpectDisabledPath(kSkipButton);
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithManagedLocalPinAndPasswordEnabled,
                       SAMLSkipButtonVisibleWhenLocalPasswordAndPinAllowed) {
  // Policy allows PIN and Local Password.
  SetPinAndLocalPasswordAsAllowedAuthFactorsForEnterpriseUsers();

  LoginAndWaitForCryptohomeSetupScreenExit();
  SetSAMLAuthFlow();
  CryptohomeRecoverySetupContinue();
  WaitForScreenShown();

  // Skip button should be enabled for SAML users when local password is
  // allowed.
  test::OobeJS().ExpectEnabledPath(kSkipButton);
}

// Test fixture for PIN complexity policies during OOBE setup.
class PinSetupScreenComplexityTest : public PinSetupScreenTest {
 public:
  PinSetupScreenComplexityTest() = default;
  ~PinSetupScreenComplexityTest() override = default;

  void SetComplexityPolicy(ash::LocalAuthFactorsComplexity complexity) {
    ProfileManager::GetActiveUserProfile()->GetPrefs()->SetInteger(
        ash::prefs::kLocalAuthFactorsComplexity, static_cast<int>(complexity));
  }
};

// Tests that when the policy is NOT set, the system correctly falls back
// to the legacy flow where weak PINs trigger a warning but allow submission.
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest,
                       LegacyFlowUsedWhenPolicyUnset) {
  ShowPinSetupScreen();
  // Do NOT set the complexity policy, leaving it as kUnset.
  WaitForScreenShown();

  // Enter a weak PIN.
  EnterPin(kWeakPin);

  // In the legacy flow, a weak PIN generates a WARNING but still allows the
  // user to submit and set the PIN. The 'Next' button should be enabled.
  WaitUntilNextButtonEnabled(true);

  // Verify the user can proceed to the confirmation step.
  TapNextButton();
  WaitUntilConfirmationStep();
}

// Tests that a weak PIN is blocked by the complexity policy and prevents
// the user from continuing.
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest, WeakPinBlocked) {
  ShowPinSetupScreen();
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  WaitForScreenShown();

  // Enter a weak PIN.
  EnterPin(kWeakPin);

  // The complexity check is asynchronous. Wait for the 'Next' button to become
  // disabled.
  WaitUntilNextButtonEnabled(false);
}

// Tests that a weak PIN explicitly displays the correct error message on the UI
// when blocked by the complexity policy.
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest, WeakPinShowsErrorMessage) {
  ShowPinSetupScreen();
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  WaitForScreenShown();

  // Enter a weak PIN.
  EnterPin(kWeakPin);

  // Verify the hard error state and message.
  ExpectProblemMessage(/*is_error=*/true, kExpectedHighComplexityError);
}

// Tests that a weak PIN explicitly displays the correct warning message on
// the UI when the complexity policy is unset (legacy flow).
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest,
                       LegacyFlowWeakPinShowsWarningMessage) {
  ShowPinSetupScreen();
  // Do NOT set the complexity policy, leaving it as kUnset.
  WaitForScreenShown();

  // Enter a weak PIN.
  EnterPin(kWeakPin);

  // In the legacy flow, a weak PIN generates a WARNING but still allows the
  // user to submit. The 'Next' button should be enabled.
  WaitUntilNextButtonEnabled(true);

  // Verify the warning state (not an error) and the legacy message.
  ExpectProblemMessage(/*is_error=*/false, kExpectedLegacyWeakPinWarning);
}

// Tests that a strong PIN passes the complexity policy and allows
// the user to continue to the confirmation step.
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest, StrongPinAllowed) {
  ShowPinSetupScreen();
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  WaitForScreenShown();

  // Enter a known strong PIN that passes the high complexity check.
  EnterPin(kStrongPin);

  // Wait for the asynchronous check to succeed and enable the 'Next' button.
  WaitUntilNextButtonEnabled(true);

  // Verify the user can actually proceed to the confirmation step.
  TapNextButton();
  WaitUntilConfirmationStep();
}

// Tests that entering a weak PIN shows an error, but subsequently entering
// a strong PIN clears the error and allows submission.
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest,
                       WeakThenStrongPinClearsError) {
  ShowPinSetupScreen();
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kHigh);
  WaitForScreenShown();

  // 1. Enter a weak PIN and verify it gets blocked.
  EnterPin(kWeakPin);
  WaitUntilNextButtonEnabled(false);
  ExpectProblemMessage(/*is_error=*/true, kExpectedHighComplexityError);

  // 2. Enter a strong PIN and verify the UI recovers.
  EnterPin(kStrongPin);

  // The asynchronous check should succeed, enabling the button and hiding the
  // error.
  WaitUntilNextButtonEnabled(true);
  ExpectNoProblemMessage();

  // Verify the user can actually proceed to the confirmation step.
  TapNextButton();
  WaitUntilConfirmationStep();
}

// Tests that navigating back from the confirmation step to the setup step
// correctly preserves the complexity-based requirement message (4 digits) and
// does not revert to the legacy requirement message (6 digits).
IN_PROC_BROWSER_TEST_F(PinSetupScreenComplexityTest,
                       BackButtonPreservesComplexityMessage) {
  ShowPinSetupScreen();
  SetComplexityPolicy(ash::LocalAuthFactorsComplexity::kLow);
  WaitForScreenShown();

  // Wait for the complexity policy to be fetched and the UI to update.
  test::OobeJS()
      .CreateWaiter(base::StrCat({test::GetOobeElementPath(kProblemDiv),
                                  ".textContent.includes('4 digits')"}))
      ->Wait();

  EnterPin("1234");
  TapNextButton();

  // Wait for the UI to transition to the confirmation step.
  test::OobeJS().CreateVisibilityWaiter(true, kBackButton)->Wait();

  // Navigate back to the first step.
  test::OobeJS().ClickOnPath(kBackButton);

  // Wait for the UI to transition back.
  test::OobeJS().CreateVisibilityWaiter(false, kBackButton)->Wait();

  // Verification: It must still show the 4-digit requirement.
  test::OobeJS()
      .CreateWaiter(base::StrCat({test::GetOobeElementPath(kProblemDiv),
                                  ".textContent.includes('4 digits')"}))
      ->Wait();
}

}  // namespace ash
