// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionUpdateScrollDirection[] = "update-scroll";
constexpr const char kUserActionReturn[] = "return";

bool ShouldShowChoobeReturnButton(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return false;
  }
  return controller->ShouldShowReturnButton(
      TouchpadScrollScreenView::kScreenId);
}

void ReportScreenCompletedToChoobe(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return;
  }
  controller->OnScreenCompleted(
      *ProfileManager::GetActiveUserProfile()->GetPrefs(),
      TouchpadScrollScreenView::kScreenId);
}

void RecordSettingChangedMetric(bool initial_value, bool current_value) {
  base::UmaHistogramBoolean("OOBE.CHOOBE.SettingChanged.Touchpad-scroll",
                            initial_value != current_value);
}

bool CheckNoTouchpadDeviceExist() {
  const auto touchpads =
      InputDeviceSettingsController::Get()->GetConnectedTouchpads();
  return touchpads.empty();
}

}  // namespace

// static
std::string TouchpadScrollScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

TouchpadScrollScreen::TouchpadScrollScreen(
    base::WeakPtr<TouchpadScrollScreenView> view,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(TouchpadScrollScreenView::kScreenId,
                 OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

TouchpadScrollScreen::~TouchpadScrollScreen() = default;

bool TouchpadScrollScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  if (chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    return true;
  }

  if (CheckNoTouchpadDeviceExist()) {
    return true;
  }

  if (features::IsOobeTouchpadScrollEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller && choobe_controller->ShouldScreenBeSkipped(
                                 TouchpadScrollScreenView::kScreenId)) {
      return true;
    }
  }

  // Skip the screen if `kShowTouchpadScrollScreenEnabled` preference is set by
  // admin to false.
  const PrefService::Preference* pref =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->FindPreference(
          prefs::kShowTouchpadScrollScreenEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool()) {
    return true;
  }

  return false;
}

bool TouchpadScrollScreen::MaybeSkip(WizardContext& context) {
  if (!ShouldBeSkipped(context)) {
    return false;
  }

  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void TouchpadScrollScreen::OnScrollUpdate(bool is_reverse_scroll) {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  // The pref is true if touchpad reverse scroll is enabled.
  profile->GetPrefs()->SetBoolean(prefs::kNaturalScroll, is_reverse_scroll);

  if (features::IsInputDeviceSettingsSplitEnabled()) {
    const auto touchpads =
        InputDeviceSettingsController::Get()->GetConnectedTouchpads();
    for (const auto& touchpad : touchpads) {
      const auto old_settings = std::move(touchpad->settings);
      old_settings->reverse_scrolling = is_reverse_scroll;
      InputDeviceSettingsController::Get()->SetTouchpadSettings(
          touchpad->id, old_settings->Clone());
    }
  }
}

bool TouchpadScrollScreen::GetNaturalScrollPrefValue() {
  Profile* profile = ProfileManager::GetActiveUserProfile();
  bool is_reverse_scrolling =
      profile->GetPrefs()->GetBoolean(prefs::kNaturalScroll);

  // set the synced value with the new InputDeviceSettingSplit
  // TODO(b/287376458) remove it when input team fix the sync value issue
  OnScrollUpdate(is_reverse_scrolling);
  return is_reverse_scrolling;
}

void TouchpadScrollScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  initial_pref_value_ = GetNaturalScrollPrefValue();
  view_->SetReverseScrolling(GetNaturalScrollPrefValue());

  base::Value::Dict data;
  data.Set(
      "shouldShowReturn",
      ShouldShowChoobeReturnButton(
          WizardController::default_controller()->choobe_flow_controller()));
  view_->Show(std::move(data));
}

void TouchpadScrollScreen::HideImpl() {}

void TouchpadScrollScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();

  if (action_id == kUserActionNext) {
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller());
    RecordSettingChangedMetric(initial_pref_value_,
                               GetNaturalScrollPrefValue());
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionReturn) {
    context()->return_to_choobe_screen = true;
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller());
    RecordSettingChangedMetric(initial_pref_value_,
                               GetNaturalScrollPrefValue());
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionUpdateScrollDirection) {
    CHECK_EQ(args.size(), 2u);
    OnScrollUpdate(args[1].GetBool());
    return;
  }

  BaseScreen::OnUserAction(args);
}

std::string TouchpadScrollScreen::RetrieveChoobeSubtitle() {
  if (GetNaturalScrollPrefValue()) {
    return "choobeTouchpadScrollSubtitleEnabled";
  }
  return "choobeTouchpadScrollSubtitleDisabled";
}

ScreenSummary TouchpadScrollScreen::GetScreenSummary() {
  ScreenSummary summary;
  summary.screen_id = TouchpadScrollScreenView::kScreenId;
  summary.icon_id = "oobe-40:scroll-choobe";
  summary.title_id = "choobeTouchpadScrollTitle";
  summary.is_revisitable = true;
  summary.is_synced = !ProfileManager::GetActiveUserProfile()
                           ->GetPrefs()
                           ->FindPreference(prefs::kNaturalScroll)
                           ->IsDefaultValue();
  if (summary.is_synced ||
      (WizardController::default_controller()
           ->choobe_flow_controller()
           ->IsScreenCompleted(TouchpadScrollScreenView::kScreenId))) {
    summary.subtitle_resource = RetrieveChoobeSubtitle();
  }

  return summary;
}

}  // namespace ash
