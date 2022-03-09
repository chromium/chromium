// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/constants/ash_switches.h"
#include "base/test/scoped_command_line.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/browser/ash/login/test/oobe_base_test.h"
#include "chrome/browser/ash/login/test/oobe_screen_exit_waiter.h"
#include "chrome/browser/ash/login/test/oobe_screen_waiter.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/smart_privacy_protection_screen_handler.h"
#include "content/public/test/browser_test.h"

namespace ash {

namespace {

constexpr char kSmartPrivacyProtection[] = "smart-privacy-protection";
const test::UIPath kQuickDimSection = {kSmartPrivacyProtection,
                                       "quickDimSection"};
const test::UIPath kSnoopingDetectionSection = {kSmartPrivacyProtection,
                                                "snoopingDetectionSection"};
const test::UIPath kNoThanksButton = {kSmartPrivacyProtection,
                                      "noThanksButton"};
const test::UIPath kTurnOnButton = {kSmartPrivacyProtection, "turnOnButton"};

}  // namespace

// Class to test SmartPrivacyProtection screen in OOBE. Screen promotes two
// different features and users can either turn both of them on and proceed with
// a kTurnOnButton or reject them together and proceed with kNoThanksButton.
// Features are implemented under two separate feature flags. TestMode
// represents which one of the features is enabled.
class SmartPrivacyProtectionScreenTest
    : public OobeBaseTest,
      public ::testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SmartPrivacyProtectionScreenTest() {
    std::vector<base::Feature> enabled_features;
    std::vector<base::Feature> disabled_features;
    if (std::get<0>(GetParam())) {
      enabled_features.push_back(features::kSnoopingProtection);
    } else {
      disabled_features.push_back(features::kSnoopingProtection);
    }
    if (std::get<1>(GetParam())) {
      enabled_features.push_back(features::kQuickDim);
    } else {
      disabled_features.push_back(features::kQuickDim);
    }
    scoped_feature_list_.InitWithFeatures(enabled_features, disabled_features);
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
  base::test::ScopedFeatureList scoped_feature_list_;
  base::test::ScopedCommandLine scoped_command_line_;
  LoginManagerMixin login_manager_mixin_{&mixin_host_};
};

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOnFeature) {
  ShowSmartPrivacyProtectionScreen();
  bool snooping_protection_enabled = std::get<0>(GetParam());
  bool quick_dim_enabled = std::get<1>(GetParam());
  if (!snooping_protection_enabled && !quick_dim_enabled) {
    ExitScreenAndExpectResult(
        SmartPrivacyProtectionScreen::Result::NOT_APPLICABLE);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  if (snooping_protection_enabled) {
    test::OobeJS().ExpectVisiblePath(kSnoopingDetectionSection);
  } else {
    test::OobeJS().ExpectHiddenPath(kSnoopingDetectionSection);
  }
  if (quick_dim_enabled) {
    test::OobeJS().ExpectVisiblePath(kQuickDimSection);
  } else {
    test::OobeJS().ExpectHiddenPath(kQuickDimSection);
  }
  test::OobeJS().ClickOnPath(kTurnOnButton);
  ExitScreenAndExpectResult(
      SmartPrivacyProtectionScreen::Result::PROCEED_WITH_FEATURE_ON);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
                  prefs::kSnoopingProtectionEnabled) ==
              snooping_protection_enabled);
  EXPECT_TRUE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
                  prefs::kPowerQuickDimEnabled) == quick_dim_enabled);
}

IN_PROC_BROWSER_TEST_P(SmartPrivacyProtectionScreenTest, TurnOffFeature) {
  ShowSmartPrivacyProtectionScreen();
  if (!std::get<0>(GetParam()) && !std::get<1>(GetParam())) {
    ExitScreenAndExpectResult(
        SmartPrivacyProtectionScreen::Result::NOT_APPLICABLE);
    return;
  }
  OobeScreenWaiter(SmartPrivacyProtectionView::kScreenId).Wait();
  test::OobeJS().ClickOnPath(kNoThanksButton);
  ExitScreenAndExpectResult(
      SmartPrivacyProtectionScreen::Result::PROCEED_WITH_FEATURE_OFF);
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kSnoopingProtectionEnabled));
  EXPECT_FALSE(ProfileManager::GetActiveUserProfile()->GetPrefs()->GetBoolean(
      prefs::kPowerQuickDimEnabled));
}

INSTANTIATE_TEST_SUITE_P(All,
                         SmartPrivacyProtectionScreenTest,
                         ::testing::Combine(::testing::Bool(),
                                            ::testing::Bool()));

}  // namespace ash
