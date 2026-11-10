// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/theme_selection_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/system/scheduled_feature/scheduled_feature.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/ash/login/screens/base_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/theme_selection_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/user.h"
#include "ui/native_theme/native_theme.h"

namespace ash {

namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionSelect[] = "select";
constexpr const char kUserActionReturn[] = "return";

ThemeSelectionScreen::SelectedTheme GetSelectedTheme(
    const PrefService& pref_service) {
  if (pref_service.GetInteger(prefs::kDarkModeScheduleType) ==
      static_cast<int>(ScheduleType::kSunsetToSunrise)) {
    return ThemeSelectionScreen::SelectedTheme::kAuto;
  }

  if (pref_service.GetBoolean(prefs::kDarkModeEnabled)) {
    return ThemeSelectionScreen::SelectedTheme::kDark;
  }
  return ThemeSelectionScreen::SelectedTheme::kLight;
}

std::string GetSelectedThemeString(ThemeSelectionScreen::SelectedTheme theme) {
  switch (theme) {
    case ThemeSelectionScreen::SelectedTheme::kAuto:
      return ThemeSelectionScreenView::kAutoMode;
    case ThemeSelectionScreen::SelectedTheme::kDark:
      return ThemeSelectionScreenView::kDarkMode;
    case ThemeSelectionScreen::SelectedTheme::kLight:
      return ThemeSelectionScreenView::kLightMode;
  }
}

void RecordSelectedTheme(ThemeSelectionScreen::SelectedTheme selected_theme,
                         ThemeSelectionScreen::SelectedTheme initial_theme) {
  base::UmaHistogramEnumeration("OOBE.ThemeSelectionScreen.SelectedTheme",
                                selected_theme);
  base::UmaHistogramBoolean("OOBE.CHOOBE.SettingChanged.Theme-selection",
                            selected_theme != initial_theme);
}

bool ShouldShowChoobeReturnButton(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return false;
  }
  return controller->ShouldShowReturnButton(
      ThemeSelectionScreenView::kScreenId);
}

void ReportScreenCompletedToChoobe(ChoobeFlowController* controller,
                                   PrefService& pref_service) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return;
  }
  controller->OnScreenCompleted(pref_service,
                                ThemeSelectionScreenView::kScreenId);
}

}  // namespace

// static
std::string ThemeSelectionScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kProceed:
      return "Proceed";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

ThemeSelectionScreen::ThemeSelectionScreen(
    base::WeakPtr<ThemeSelectionScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(ThemeSelectionScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ThemeSelectionScreen::~ThemeSelectionScreen() = default;

std::string ThemeSelectionScreen::RetrieveChoobeSubtitle(
    const PrefService& pref_service) {
  ThemeSelectionScreen::SelectedTheme theme = GetSelectedTheme(pref_service);
  switch (theme) {
    case ThemeSelectionScreen::SelectedTheme::kAuto:
      return "autoThemeLabel";
    case ThemeSelectionScreen::SelectedTheme::kDark:
      return "darkThemeLabel";
    case ThemeSelectionScreen::SelectedTheme::kLight:
      return "lightThemeLabel";
  }
}

bool ThemeSelectionScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests)
    return true;

  const PrefService::Preference* pref =
      CHECK_DEREF(CHECK_DEREF(context.onboarding_user.get()).GetProfilePrefs())
          .FindPreference(prefs::kDarkModeScheduleType);
  if (pref->IsManaged() || pref->IsRecommended()) {
    return true;
  }

  if (features::IsOobeChoobeEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller && choobe_controller->ShouldScreenBeSkipped(
                                 ThemeSelectionScreenView::kScreenId)) {
      return true;
    }
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

  // The context is only set while the screen is shown, which it is here.
  initial_theme_ = GetSelectedTheme(CHECK_DEREF(
      CHECK_DEREF(context()->onboarding_user.get()).GetProfilePrefs()));

  base::DictValue data;
  data.Set("selectedTheme", GetSelectedThemeString(initial_theme_));
  data.Set(
      "shouldShowReturn",
      ShouldShowChoobeReturnButton(
          WizardController::default_controller()->choobe_flow_controller()));
  view_->Show(std::move(data));
}

void ThemeSelectionScreen::HideImpl() {}

void ThemeSelectionScreen::OnUserAction(const base::ListValue& args) {
  // The screen is up whenever a user action arrives, so the context is set.
  PrefService& pref_service = CHECK_DEREF(
      CHECK_DEREF(context()->onboarding_user.get()).GetProfilePrefs());
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionSelect) {
    const SelectedTheme selected_theme =
        static_cast<SelectedTheme>(args[1].GetInt());

    if (selected_theme == SelectedTheme::kAuto) {
      pref_service.SetInteger(prefs::kDarkModeScheduleType,
                              static_cast<int>(ScheduleType::kSunsetToSunrise));
    } else {
      pref_service.SetInteger(prefs::kDarkModeScheduleType,
                              static_cast<int>(ScheduleType::kNone));
      pref_service.SetBoolean(prefs::kDarkModeEnabled,
                              selected_theme == SelectedTheme::kDark);
    }
  } else if (action_id == kUserActionNext) {
    RecordSelectedTheme(GetSelectedTheme(pref_service), initial_theme_);
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller(),
        pref_service);
    exit_callback_.Run(Result::kProceed);
  } else if (action_id == kUserActionReturn) {
    context()->return_to_choobe_screen = true;
    RecordSelectedTheme(GetSelectedTheme(pref_service), initial_theme_);
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller(),
        pref_service);
    exit_callback_.Run(Result::kProceed);
    return;
  } else {
    BaseScreen::OnUserAction(args);
  }
}

ScreenSummary ThemeSelectionScreen::GetScreenSummary(
    const WizardContext& context) {
  PrefService& pref_service =
      CHECK_DEREF(CHECK_DEREF(context.onboarding_user.get()).GetProfilePrefs());
  ScreenSummary summary;
  summary.screen_id = ThemeSelectionScreenView::kScreenId;
  summary.icon_id = "oobe-40:theme-choobe";
  summary.title_id = "choobeThemeSelectionTitle";
  summary.is_revisitable = true;
  summary.is_synced = false;

  if (WizardController::default_controller()
          ->choobe_flow_controller()
          ->IsScreenCompleted(ThemeSelectionScreenView::kScreenId)) {
    summary.subtitle_resource = RetrieveChoobeSubtitle(pref_service);
  }

  return summary;
}

}  // namespace ash
