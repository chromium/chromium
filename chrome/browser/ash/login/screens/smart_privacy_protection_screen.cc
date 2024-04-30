// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/smart_privacy_protection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/smart_privacy_protection_screen_handler.h"
#include "components/prefs/pref_service.h"

namespace ash {

namespace {

constexpr const char kUserActionFeatureTurnOn[] = "continue-feature-on";
constexpr const char kUserActionFeatureTurnOff[] = "continue-feature-off";
constexpr const char kUserActionShowLearnMore[] = "show-learn-more";

}  // namespace

// static
std::string SmartPrivacyProtectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kProceedWithFeatureOn:
      return "ContinueWithFeatureOn";
    case Result::kProceedWithFeatureOff:
      return "ContinueWithFeatureOff";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

SmartPrivacyProtectionScreen::SmartPrivacyProtectionScreen(
    base::WeakPtr<SmartPrivacyProtectionView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(SmartPrivacyProtectionView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

SmartPrivacyProtectionScreen::~SmartPrivacyProtectionScreen() = default;

bool SmartPrivacyProtectionScreen::MaybeSkip(WizardContext& context) {
  // SmartPrivacyProtectionScreen lets user set two settings simultaneously:
  // SnoopingProtection and QuickDim. The screen should be skipped if none of
  // them is enabled.
  if (!context.skip_post_login_screens_for_tests &&
      features::IsQuickDimEnabled() && !DemoSession::IsDeviceInDemoMode()) {
    return false;
  }
  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void SmartPrivacyProtectionScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void SmartPrivacyProtectionScreen::HideImpl() {}

void SmartPrivacyProtectionScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionFeatureTurnOn) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    profile->GetPrefs()->SetBoolean(prefs::kPowerQuickDimEnabled,
                                    features::IsQuickDimEnabled());
    exit_callback_.Run(Result::kProceedWithFeatureOn);
  } else if (action_id == kUserActionFeatureTurnOff) {
    Profile* profile = ProfileManager::GetActiveUserProfile();
    profile->GetPrefs()->SetBoolean(prefs::kPowerQuickDimEnabled, false);
    exit_callback_.Run(Result::kProceedWithFeatureOff);
  } else if (action_id == kUserActionShowLearnMore) {
    // TODO(crbug.com/1293320): add p-link once available
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
