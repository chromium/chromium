// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/theme_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/chromeos/login/theme_selection_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"

namespace ash {

namespace {
constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSelect[] = "select";
}  // namespace

// static
std::string ThemeSelectionScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kProceed:
      return "Proceed";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
}

ThemeSelectionScreen::ThemeSelectionScreen(
    base::WeakPtr<ThemeSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ThemeSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ThemeSelectionScreen::~ThemeSelectionScreen() = default;

bool ThemeSelectionScreen::MaybeSkip(WizardContext* context) {
  if (context->skip_post_login_screens_for_tests ||
      !chromeos::features::IsOobeThemeSelectionEnabled()) {
    exit_callback_.Run(Result::kNotApplicable);
    return true;
  }
  return false;
}

void ThemeSelectionScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void ThemeSelectionScreen::HideImpl() {}

void ThemeSelectionScreen::OnUserAction(const base::Value::List& args) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSelect) {
    const SelectedTheme selected_theme =
        static_cast<SelectedTheme>(args[1].GetInt());

    profile->GetPrefs()->SetBoolean(prefs::kDarkModeEnabled,
                                    selected_theme == SelectedTheme::kDark);

    if (selected_theme == SelectedTheme::kAuto) {
      profile->GetPrefs()->SetInteger(
          prefs::kDarkModeScheduleType,
          static_cast<int>(ScheduledFeature::ScheduleType::kSunsetToSunrise));
    } else {
      profile->GetPrefs()->SetInteger(
          prefs::kDarkModeScheduleType,
          static_cast<int>(ScheduledFeature::ScheduleType::kNone));
    }

  } else if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kProceed);

  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
