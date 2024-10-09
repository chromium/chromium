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
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/screen_manager.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/js_checker.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/fingerprint_setup_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/password_selection_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/pin_setup_screen_handler.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/cryptohome/constants.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "components/user_manager/user_type.h"
#include "content/public/test/browser_test.h"
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

// PasswordSelectionScreen elements.
const test::UIPath kGaiaPasswordButton = {"password-selection",
                                          "gaiaPasswordButton"};
const test::UIPath kNextButtonPasswordSelection = {"password-selection",
                                                   "nextButton"};

enum class PinPolicy {
  kUnlock,
  kWebAuthn,
};

enum class AllowlistStatus {
  kPin,
  kAll,
  kNone,
};

// Utility function for setting relevant policy affecting PIN behavior.
void SetPinPolicy(PinPolicy policy, AllowlistStatus desired_status) {
  base::Value::List allowlist_status;
  switch (desired_status) {
    case AllowlistStatus::kPin:
      allowlist_status.Append(base::Value("PIN"));
      break;
    case AllowlistStatus::kAll:
      allowlist_status.Append(base::Value("all"));
      break;
    case AllowlistStatus::kNone:
      break;
  }

  PrefService* prefs = ProfileManager::GetActiveUserProfile()->GetPrefs();
  const auto* associated_pref = policy == PinPolicy::kWebAuthn
                                    ? prefs::kWebAuthnFactors
                                    : prefs::kQuickUnlockModeAllowlist;
  prefs->SetList(associated_pref, std::move(allowlist_status));
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

  void SetUpOnMainThread() override {
    OobeBaseTest::SetUpOnMainThread();

    original_callback_ = GetScreen()->get_exit_callback_for_testing();
    GetScreen()->set_exit_callback_for_testing(base::BindRepeating(
        &PinSetupScreenTest::HandleScreenExit, base::Unretained(this)));

    original_fingerprint_callback_ =
        GetFingerprintScreen()->get_exit_callback_for_testing();
    GetFingerprintScreen()->set_exit_callback_for_testing(
        base::BindRepeating(&PinSetupScreenTest::HandleFingerprintScreenExit,
                            base::Unretained(this)));

    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    wizard_context->knowledge_factor_setup.pin_setup_mode =
        WizardContext::PinSetupMode::kSetupAsSecondaryFactor;

    // Force the sync screen to be shown so that we don't jump to PIN setup
    // screen (consuming auth session) in unbranded build
    wizard_context->is_branded_build = true;

    login_manager_mixin_.LoginAsNewRegularUser();

    // Add an authenticated session to the user context used during OOBE. In
    // production, this is set by earlier screens which are skipped in this
    // test.
    std::unique_ptr<UserContext> context =
        ash::AuthSessionStorage::Get()->BorrowForTests(
            FROM_HERE, wizard_context->extra_factors_token.value());
    // LoginManagerMixin uses StubAuthenticator that fills out authsession.
    // Reset Authsession to correctly interact with FakeUserDataAuthClient.
    context->ResetAuthSessionIds();
    cryptohome_.MarkUserAsExisting(context->GetAccountId());
    auto session_ids =
        cryptohome_.AddSession(context->GetAccountId(), /*authenticated=*/true);
    context->SetAuthSessionIds(session_ids.first, session_ids.second);
    context->SetSessionLifetime(base::Time::Now() +
                                cryptohome::kAuthsessionInitialLifetime);
    ash::AuthSessionStorage::Get()->Return(
        wizard_context->extra_factors_token.value(), std::move(context));
  }

  PinSetupScreen* GetScreen() {
    return WizardController::default_controller()->GetScreen<PinSetupScreen>();
  }

  FingerprintSetupScreen* GetFingerprintScreen() {
    return WizardController::default_controller()
        ->GetScreen<FingerprintSetupScreen>();
  }

  void EnterPin() { test::OobeJS().TypeIntoPath("654321", kPinKeyboardInput); }

  void ShowPinSetupScreen() {
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    if (!screen_exited_) {
      LoginDisplayHost::default_host()->StartWizard(
          PinSetupScreenView::kScreenId);
    }
  }

  void WaitForScreenShown() {
    OobeScreenWaiter(PinSetupScreenView::kScreenId).Wait();
  }

  void TapSkipButton() { test::OobeJS().TapOnPath(kSkipButton); }

  void TapNextButton() { test::OobeJS().TapOnPath(kNextButton); }

  void TapDoneButton() {
    test::OobeJS()
        .CreateVisibilityWaiter(true, kPinSetupScreenDoneStep)
        ->Wait();
    test::OobeJS().TapOnPath(kDoneButton);
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

  void WaitForScreenExit() {
    if (screen_exited_)
      return;
    base::RunLoop run_loop;
    screen_exit_callback_ = run_loop.QuitClosure();
    run_loop.Run();
  }

  void WaitForFingerprintScreenExit() {
    if (!fingerprint_screen_result_.has_value()) {
      base::RunLoop run_loop;
      fingerprint_screen_exit_callback_ = run_loop.QuitClosure();
      run_loop.Run();
    }
  }

  void ConfigureUserContextForTest() {
    std::unique_ptr<UserContext> context = std::make_unique<UserContext>();
    context->SetAuthSessionIds("fake-session-id", "broadcast");
    context->SetSessionLifetime(base::Time::Now() +
                                cryptohome::kAuthsessionInitialLifetime);
    LoginDisplayHost::default_host()
        ->GetWizardContextForTesting()
        ->extra_factors_token =
        ash::AuthSessionStorage::Get()->Store(std::move(context));
  }

  void CheckCredentialsWereCleared() {
    ExpectExtraFactorsTokenPresence(/*present=*/false);
  }

  void CheckCredentialsArePresent() {
    ExpectExtraFactorsTokenPresence(/*present=*/true);
  }

  void ExpectExtraFactorsTokenPresence(bool present) {
    EXPECT_EQ(LoginDisplayHost::default_host()
                  ->GetWizardContextForTesting()
                  ->extra_factors_token.has_value(),
              present);
  }

  void ExpectUserActionMetric(PinSetupScreen::UserAction user_action) {
    EXPECT_THAT(
        histogram_tester_.GetAllSamples(kPinSetupScreenUserAction),
        ElementsAre(base::Bucket(static_cast<int>(user_action), /*count=*/1)));
  }

  void ExpectExitResultAndMetric(PinSetupScreen::Result result) {
    EXPECT_EQ(screen_result_.value(), result);

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

  void ExpectFingerprintScreenExitedAndContinue() {
    EXPECT_EQ(fingerprint_screen_result_.value(),
              FingerprintSetupScreen::Result::NOT_APPLICABLE);
    original_fingerprint_callback_.Run(
        FingerprintSetupScreen::Result::NOT_APPLICABLE);
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

  std::optional<PinSetupScreen::Result> screen_result_;
  std::optional<FingerprintSetupScreen::Result> fingerprint_screen_result_;
  base::HistogramTester histogram_tester_;
  bool screen_exited_ = false;

  LoginManagerMixin login_manager_mixin_{&mixin_host_};
  CryptohomeMixin cryptohome_{&mixin_host_};

 private:
  void HandleScreenExit(PinSetupScreen::Result result) {
    screen_exited_ = true;
    screen_result_ = result;
    original_callback_.Run(result);
    if (screen_exit_callback_)
      std::move(screen_exit_callback_).Run();
  }

  void HandleFingerprintScreenExit(FingerprintSetupScreen::Result result) {
    fingerprint_screen_result_ = result;
    if (fingerprint_screen_exit_callback_) {
      std::move(fingerprint_screen_exit_callback_).Run();
    }
  }

  PinSetupScreen::ScreenExitCallback original_callback_;
  base::RepeatingClosure screen_exit_callback_;

  // For inspecting the exit behavior of the fingerprint setup screen.
  FingerprintSetupScreen::ScreenExitCallback original_fingerprint_callback_;
  base::RepeatingClosure fingerprint_screen_exit_callback_;
};

// By default, OOBE shows the PIN setup screen on supported hardware.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShownByDefault) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
}

// The screen should be skipped when the 'extra_factors_token' isn't present.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, SkipWhenExtraFactorsTokenMissing) {
  LoginDisplayHost::default_host()
      ->GetWizardContextForTesting()
      ->extra_factors_token->clear();

  ShowPinSetupScreen();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kNotApplicable);
}

// The screen should be skipped when the token is invalid.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, SkipWhenTokenInvalid) {
  ash::AuthSessionStorage::Get()->Invalidate(LoginDisplayHost::default_host()
                                                 ->GetWizardContextForTesting()
                                                 ->extra_factors_token.value(),
                                             base::DoNothing());

  ShowPinSetupScreen();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kNotApplicable);
}

// If the PIN setup screen is shown, auth session should be cleared afterwards.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, AuthSessionIsClearedOnManualSkip) {
  ConfigureUserContextForTest();

  ShowPinSetupScreen();
  WaitForScreenShown();
  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
  CheckCredentialsWereCleared();
}

// Oobe should skip the PIN setup screen if policies are set such that PIN
// cannot be used for both login/unlock and web authn.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, SkipWhenNotAllowedByPolicy) {
  SetPinPolicy(PinPolicy::kUnlock, AllowlistStatus::kNone);
  SetPinPolicy(PinPolicy::kWebAuthn, AllowlistStatus::kNone);

  ShowPinSetupScreen();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kNotApplicable);
}

// The PIN screen should be shown when policy allows PIN for unlock.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShowWhenPinAllowedForUnlock) {
  SetPinPolicy(PinPolicy::kUnlock, AllowlistStatus::kPin);
  SetPinPolicy(PinPolicy::kWebAuthn, AllowlistStatus::kNone);
  ShowPinSetupScreen();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
}

// The PIN screen should be shown when policy allows PIN for WebAuthN.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ShowWhenPinAllowedForWebAuthn) {
  SetPinPolicy(PinPolicy::kUnlock, AllowlistStatus::kNone);
  SetPinPolicy(PinPolicy::kWebAuthn, AllowlistStatus::kAll);
  ShowPinSetupScreen();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
}

// Skip the flow in the beginning and expect the proper metrics.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ManualSkipOnStart) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
  ExpectUserActionMetric(PinSetupScreen::UserAction::kSkipButtonClickedOnStart);
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, ManualSkipInFlow) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  EnterPin();
  TapNextButton();
  // Wait until the back button is visible to ensure that the UI is showing
  // the 'confirmation' step.
  test::OobeJS().CreateVisibilityWaiter(true, kBackButton)->Wait();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
  ExpectUserActionMetric(PinSetupScreen::UserAction::kSkipButtonClickedInFlow);
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTest, FinishedFlow) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  InsertAndConfirmPin();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kDoneAsSecondaryFactor);
  ExpectUserActionMetric(PinSetupScreen::UserAction::kDoneButtonClicked);
  CheckCredentialsWereCleared();
}

// Ensures the correct strings when PIN is being offered not as the main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest,
                       CorrectStringsWhenPinIsNotTheMainFactor) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  WaitForSetupTitleAndSubtitle(IDS_DISCOVER_PIN_SETUP_TITLE1,
                               IDS_DISCOVER_PIN_SETUP_SUBTITLE1);
  test::OobeJS().ExpectElementText(
      l10n_util::GetStringUTF8(IDS_DISCOVER_PIN_SETUP_SKIP), kSkipButtonCore);
}

// The AuthSession should not be kept alive while offering PIN as a secondary
// factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTest,
                       AuthSessionIsNotKeptAliveForSecondaryFactorSetup) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // Ensure that there isn't a SessionRefresher keeping the AuthSession alive.
  EXPECT_FALSE(AuthSessionStorage::Get()->CheckHasKeepAliveForTesting(
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->extra_factors_token.value()));
}

// Fixture to pretend that hardware support for login is not available.
class PinSetupScreenTestWithoutLoginSupport : public PinSetupScreenTest {
 public:
  PinSetupScreenTestWithoutLoginSupport() { SetHardwareSupport(false); }

  ~PinSetupScreenTestWithoutLoginSupport() override = default;
};

// By default, OOBE should skip the PIN setup screen when hardware support is
// not available.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithoutLoginSupport,
                       SkippedByDefault) {
  ShowPinSetupScreen();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kNotApplicable);
}

IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithoutLoginSupport,
                       AuthSessionIsClearedWhenSkipped) {
  ConfigureUserContextForTest();
  ShowPinSetupScreen();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kNotApplicable);
  CheckCredentialsWereCleared();
}

// The screen should be shown for tablet devices, regardless of the hardware
// support status.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestWithoutLoginSupport,
                       ShowInTabletMode) {
  SetTabletMode(true);
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();
  WaitForScreenExit();

  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserSkip);
}

class PinSetupScreenTestAsMainFactor : public PinSetupScreenTest {
 public:
  PinSetupScreenTestAsMainFactor() {
    SetHardwareSupport(true);
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{ash::features::kAllowPasswordlessSetup},
        /*disabled_features=*/{});
    scoped_command_line_.GetProcessCommandLine()->AppendSwitch(
        ash::switches::kOobeEnablePinOnlyPrototype);
  }

  void SetUpOnMainThread() override {
    PinSetupScreenTest::SetUpOnMainThread();
    auto* wizard_context =
        LoginDisplayHost::default_host()->GetWizardContextForTesting();
    wizard_context->knowledge_factor_setup.pin_setup_mode =
        WizardContext::PinSetupMode::kSetupAsPrimaryFactor;
  }

  ~PinSetupScreenTestAsMainFactor() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
};

// Tests that the strings are correct when setting up PIN as the main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactor,
                       TitleAndSubtitleStrings) {
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
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactor,
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
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactor, MainFactorSet) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  InsertAndConfirmPin();
  WaitForScreenExit();

  // The flow will exit and continue into the fingerprint setup screen. Ensure
  // that the credentials are still present.
  CheckCredentialsArePresent();
  ExpectExitResultAndMetric(PinSetupScreen::Result::kDoneAsMainFactor);
  ExpectFingerprintScreenExitedAndContinue();

  // When the PIN is surfaced at the end of the flow for a second time, it exits
  // properly, since a PIN has already been set.
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::kNotApplicable);
  CheckCredentialsWereCleared();
}

// PIN is offered as an additional factor at the end of the auth factor setup
// flow when the user chooses not to use it as a main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactor,
                       SkippingLeadsToPinBeingOfferedAsSecondaryFactor) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  TapSkipButton();

  // The flow leads to the password selection screen. Ensure that the
  // credentials have not been cleared.
  ExpectExitResultAndMetric(PinSetupScreen::Result::kUserChosePassword);
  CheckCredentialsArePresent();
  HandlePasswordSelectionScreen();

  // Once the password is set, the flow continues into fingerprint setup.
  WaitForFingerprintScreenExit();
  ExpectFingerprintScreenExitedAndContinue();

  // Skip offering to set a PIN as an additional factor.
  WaitForScreenShown();
  TapSkipButton();
  EXPECT_EQ(screen_result_.value(), PinSetupScreen::Result::kUserSkip);
  CheckCredentialsWereCleared();
}

// Ensures that the AuthSession is kept alive when PIN is being offered as the
// main factor.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactor,
                       AuthSessionIsKeptAliveForMainFactorSetup) {
  ShowPinSetupScreen();
  WaitForScreenShown();

  // Ensure that there is a SessionRefresher keeping the AuthSession alive.
  EXPECT_TRUE(AuthSessionStorage::Get()->CheckHasKeepAliveForTesting(
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->extra_factors_token.value()));
}

class PinSetupScreenTestAsMainFactorWithoutLoginSupport
    : public PinSetupScreenTestAsMainFactor {
 public:
  PinSetupScreenTestAsMainFactorWithoutLoginSupport() {
    SetHardwareSupport(false);
  }

  ~PinSetupScreenTestAsMainFactorWithoutLoginSupport() override = default;
};

// Tests that the screen is not shown as a main factor when not supported. When
// that is the case, the password selection screen should be shown next.
IN_PROC_BROWSER_TEST_F(PinSetupScreenTestAsMainFactorWithoutLoginSupport,
                       NotShownWhenNotSupported) {
  ShowPinSetupScreen();
  WaitForScreenExit();

  // Wait for the password selection screen to be surfaced.
  ExpectExitResultAndMetric(
      PinSetupScreen::Result::kNotApplicableAsPrimaryFactor);
  OobeScreenWaiter(PasswordSelectionScreenHandler::kScreenId).Wait();
  CheckCredentialsArePresent();
}

}  // namespace ash
