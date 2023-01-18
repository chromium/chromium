// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/feature_parameter_interface.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kSmartPrivacyProtection[] = "smart-privacy-protection";
const test::UIPath kQuickDimSection = {kSmartPrivacyProtection,
                                       "quickDimSection"};
const test::UIPath kNoThanksButton = {kSmartPrivacyProtection,
                                      "noThanksButton"};
const test::UIPath kTurnOnButton = {kSmartPrivacyProtection, "turnOnButton"};

}  // namespace

// Class to test SmartPrivacyProtection screen in OOBE. Screen promotes the
// "lock on leave" feature that users can either turn and proceed with a
// kTurnOnButton or reject and proceed with a kNoThanksButton. The feature
// parameter determines whether kQuickDim is enabled or not for the test.
class SmartPrivacyProtectionScreenTest : public OobeBaseTest,
                                         public FeatureAsParameterInterface<1> {
 public:
  SmartPrivacyProtectionScreenTest() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(switches::kHasHps);
  }

  void SetUpOnMainThread() override {
    SmartPrivacyProtectionScreen* smart_privacy_screen =
        WizardController::default_controller()
            ->GetScreen<SmartPrivacyProtectionScreen>();
    smart_privacy_screen->set_exit_callback_for_testing(
        base::BindRepeating(&SmartPrivacyProtectionScreenTest::HandleScreenExit,
                            base::Unretained(this)));
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowSmartPrivacyProtectionScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        SmartPrivacyProtectionView::kScreenId);
  }

  void WaitForScreenExit() {
    if (result_.has_value())
      return;
    base::RunLoop run_loop;
    quit_closure_ = base::BindOnce(run_loop.QuitClosure());
    run_loop.Run();
  }

  void ExitScreenAndExpectResult(SmartPrivacyProtectionScreen::Result result) {
    WaitForScreenExit();
    EXPECT_TRUE(result_.has_value());
    EXPECT_EQ(result_.value(), result);
  }

  absl::optional<SmartPrivacyProtectionScreen::Result> result_;

 private:
  void HandleScreenExit(SmartPrivacyProtectionScreen::Result result) {
    result_ = result;
    if (quit_closure_)
      std::move(quit_closure_).Run();
  }

  base::OnceClosure quit_closure_;
  base::test::ScopedCommandLine scoped_command_line_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOnFeature) {
  ShowSmartPrivacyProtectionScreen();
  if (!IsFeatureEnabledInThisTestCase(features::kQuickDim)) {
    ExitScreenAndExpectResult(
        SmartPrivacyProtectionScreen::Result::NOT_APPLICABLE);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kQuickDimSection);
  test::OobeJS().ClickOnPath(kTurnOnButton);
  ExitScreenAndExpectResult(
      SmartPrivacyProtectionScreen::Result::PROCEED_WITH_FEATURE_ON);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kPowerQuickDimEnabled));
}

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOffFeature) {
  ShowSmartPrivacyProtectionScreen();
  if (!IsFeatureEnabledInThisTestCase(features::kQuickDim)) {
    ExitScreenAndExpectResult(
        SmartPrivacyProtectionScreen::Result::NOT_APPLICABLE);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(kNoThanksButton);
  ExitScreenAndExpectResult(
      SmartPrivacyProtectionScreen::Result::PROCEED_WITH_FEATURE_OFF);
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kPowerQuickDimEnabled));
}

// Note that both tests have essentially the same logic when the feature is
// disabled. We leave in the redundant logic since it will become needed once we
// add the UI for snooping protection.
const auto kAllFeatureVariations =
    FeatureAsParameterInterface<1>::Generator({&features::kQuickDim});

INSTANTIATE_TEST_SUITE_P(All,
                         SmartPrivacyProtectionScreenTest,
                         testing::ValuesIn(kAllFeatureVariations),
                         FeatureAsParameterInterface<1>::ParamInfoToString);

}  // namespace ash
