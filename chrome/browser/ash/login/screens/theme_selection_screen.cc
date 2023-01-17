// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/theme_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "ui/native_theme/native_theme.h"

namespace ash {

namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSelect[] = "select";

std::string GetSelectedTheme(Profile* profile) {
  if (profile->GetPrefs()->GetInteger(prefs::kDarkModeScheduleType) ==
      static_cast<int>(ScheduleType::kSunsetToSunrise)) {
    return ThemeSelectionScreenView::kAutoMode;
  }

  if (profile->GetPrefs()->GetBoolean(prefs::kDarkModeEnabled)) {
    return ThemeSelectionScreenView::kDarkMode;
  }
  return ThemeSelectionScreenView::kLightMode;
}

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

bool ThemeSelectionScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests)
    return true;

  const PrefService::Preference* pref =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->FindPreference(
          prefs::kDarkModeScheduleType);
  if (pref->IsManaged() || pref->IsRecommended() ||
      !features::IsDarkLightModeEnabled()) {
    return true;
  }

  if (features::IsOobeChoobeEnabled()) {
    return WizardController::default_controller()
        ->GetChoobeFlowController()
        ->ShouldScreenBeSkipped(ThemeSelectionScreenView::kScreenId);
  }

  return false;
}

bool ThemeSelectionScreen::MaybeSkip(WizardContext& context) {
  if (!ShouldBeSkipped(context))
    return false;

  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void ThemeSelectionScreen::ShowImpl() {
  if (!view_)
    return;
  Profile* profile = ProfileManager::GetActiveUserProfile();
  view_->Show(GetSelectedTheme(profile));
}

void ThemeSelectionScreen::HideImpl() {}

void ThemeSelectionScreen::OnUserAction(const base::Value::List& args) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  const std::string& action_id = args[0].GetString();

  // Set the nudge shown count to 0 once the user goes through the Dark / Light
  // setup to avoiding triggering Dark / Light nudge after OOBE.
  profile->GetPrefs()->SetInteger(prefs::kDarkLightModeNudgeLeftToShowCount, 0);

  if (action_id == kUserActionSelect) {
    const SelectedTheme selected_theme =
        static_cast<SelectedTheme>(args[1].GetInt());

    if (selected_theme == SelectedTheme::kAuto) {
      profile->GetPrefs()->SetInteger(
          prefs::kDarkModeScheduleType,
          static_cast<int>(ScheduleType::kSunsetToSunrise));
    } else {
      profile->GetPrefs()->SetInteger(prefs::kDarkModeScheduleType,
                                      static_cast<int>(ScheduleType::kNone));
      profile->GetPrefs()->SetBoolean(prefs::kDarkModeEnabled,
                                      selected_theme == SelectedTheme::kDark);
    }
  } else if (action_id == kUserActionNext) {
    exit_callback_.Run(Result::kProceed);
  } else {
    BaseScreen::OnUserAction(args);
  }
}

}  // namespace ash
