// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/command_line.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
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
    LoginDisplayHost::default_host()->GetWizardContext()->is_branded_build =
        true;
    SmartPrivacyProtectionScreen* smart_privacy_screen =
        WizardController::default_controller()
            ->GetScreen<SmartPrivacyProtectionScreen>();
    original_callback_ = smart_privacy_screen->get_exit_callback_for_testing();
    smart_privacy_screen->set_exit_callback_for_testing(
        screen_result_waiter_.GetRepeatingCallback());
    OobeBaseTest::SetUpOnMainThread();
  }

  void ShowSmartPrivacyProtectionScreen() {
    login_manager_mixin_.LoginAsNewRegularUser();
    OobeScreenExitWaiter(GetFirstSigninScreen()).Wait();
    WizardController::default_controller()->AdvanceToScreen(
        SmartPrivacyProtectionView::kScreenId);
  }

  SmartPrivacyProtectionScreen::Result WaitForScreenExitResult() {
    SmartPrivacyProtectionScreen::Result result = screen_result_waiter_.Take();
    original_callback_.Run(result);
    return result;
  }

 private:
  base::test::TestFuture<SmartPrivacyProtectionScreen::Result>
      screen_result_waiter_;
  SmartPrivacyProtectionScreen::ScreenExitCallback original_callback_;
  base::test::ScopedCommandLine scoped_command_line_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOnFeature) {
  ShowSmartPrivacyProtectionScreen();
  if (!IsFeatureEnabledInThisTestCase(features::kQuickDim)) {
    SmartPrivacyProtectionScreen::Result screen_result =
        WaitForScreenExitResult();
    EXPECT_EQ(screen_result,
              SmartPrivacyProtectionScreen::Result::kNotApplicable);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  test::OobeJS().ExpectVisiblePath(kQuickDimSection);
  test::OobeJS().ClickOnPath(kTurnOnButton);
  SmartPrivacyProtectionScreen::Result screen_result =
      WaitForScreenExitResult();
  EXPECT_EQ(screen_result,
            SmartPrivacyProtectionScreen::Result::kProceedWithFeatureOn);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kPowerQuickDimEnabled));
}

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOffFeature) {
  ShowSmartPrivacyProtectionScreen();
  if (!IsFeatureEnabledInThisTestCase(features::kQuickDim)) {
    SmartPrivacyProtectionScreen::Result screen_result =
        WaitForScreenExitResult();
    EXPECT_EQ(screen_result,
              SmartPrivacyProtectionScreen::Result::kNotApplicable);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(kNoThanksButton);
  SmartPrivacyProtectionScreen::Result screen_result =
      WaitForScreenExitResult();
  EXPECT_EQ(screen_result,
            SmartPrivacyProtectionScreen::Result::kProceedWithFeatureOff);
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
