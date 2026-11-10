// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/touchpad_scroll_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/public/cpp/input_device_settings_controller.h"
#include "ash/public/cpp/schedule_enums.h"
#include "ash/public/mojom/input_device_settings.mojom.h"
#include "base/check_deref.h"
#include "base/metrics/histogram_functions.h"
#include "base/values.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/prefs/pref_service_syncable_util.h"
#include "chrome/browser/ui/webui/ash/login/touchpad_scroll_screen_handler.h"
#include "components/prefs/pref_service.h"
#include "components/sync_preferences/pref_service_syncable.h"
#include "components/user_manager/user.h"

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

void ReportScreenCompletedToChoobe(ChoobeFlowController* controller,
                                   PrefService& pref_service) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return;
  }
  controller->OnScreenCompleted(pref_service,
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
      CHECK_DEREF(CHECK_DEREF(context.onboarding_user.get()).GetProfilePrefs())
          .FindPreference(prefs::kShowTouchpadScrollScreenEnabled);
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

void TouchpadScrollScreen::OnScrollUpdate(PrefService& pref_service,
                                          bool is_reverse_scroll) {
  // The pref is true if touchpad reverse scroll is enabled.
  pref_service.SetBoolean(prefs::kNaturalScroll, is_reverse_scroll);

  const auto touchpads =
      InputDeviceSettingsController::Get()->GetConnectedTouchpads();
  for (const auto& touchpad : touchpads) {
    const auto old_settings = std::move(touchpad->settings);
    old_settings->reverse_scrolling = is_reverse_scroll;
    InputDeviceSettingsController::Get()->SetTouchpadSettings(
        touchpad->id, old_settings->Clone());
  }
}

bool TouchpadScrollScreen::GetNaturalScrollPrefValue(
    PrefService& pref_service) {
  bool is_reverse_scrolling = pref_service.GetBoolean(prefs::kNaturalScroll);

  // set the synced value with the new InputDeviceSettingSplit
  // TODO(b/287376458) remove it when input team fix the sync value issue
  OnScrollUpdate(pref_service, is_reverse_scrolling);
  return is_reverse_scrolling;
}

void TouchpadScrollScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // The context is only set while the screen is shown, which it is here.
  PrefService& pref_service = CHECK_DEREF(
      CHECK_DEREF(context()->onboarding_user.get()).GetProfilePrefs());
  initial_pref_value_ = GetNaturalScrollPrefValue(pref_service);
  view_->SetReverseScrolling(GetNaturalScrollPrefValue(pref_service));

  base::DictValue data;
  data.Set(
      "shouldShowReturn",
      ShouldShowChoobeReturnButton(
          WizardController::default_controller()->choobe_flow_controller()));
  view_->Show(std::move(data));
}

void TouchpadScrollScreen::HideImpl() {}

void TouchpadScrollScreen::OnUserAction(const base::ListValue& args) {
  const std::string& action_id = args[0].GetString();

  // The screen is up whenever a user action arrives, so the context is set.
  PrefService& pref_service = CHECK_DEREF(
      CHECK_DEREF(context()->onboarding_user.get()).GetProfilePrefs());

  if (action_id == kUserActionNext) {
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller(),
        pref_service);
    RecordSettingChangedMetric(initial_pref_value_,
                               GetNaturalScrollPrefValue(pref_service));
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionReturn) {
    context()->return_to_choobe_screen = true;
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller(),
        pref_service);
    RecordSettingChangedMetric(initial_pref_value_,
                               GetNaturalScrollPrefValue(pref_service));
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionUpdateScrollDirection) {
    CHECK_EQ(args.size(), 2u);
    OnScrollUpdate(pref_service, args[1].GetBool());
    return;
  }

  BaseScreen::OnUserAction(args);
}

std::string TouchpadScrollScreen::RetrieveChoobeSubtitle(
    PrefService& pref_service) {
  if (GetNaturalScrollPrefValue(pref_service)) {
    return "choobeTouchpadScrollSubtitleEnabled";
  }
  return "choobeTouchpadScrollSubtitleDisabled";
}

ScreenSummary TouchpadScrollScreen::GetScreenSummary(
    const WizardContext& context) {
  PrefService& pref_service =
      CHECK_DEREF(CHECK_DEREF(context.onboarding_user.get()).GetProfilePrefs());
  ScreenSummary summary;
  summary.screen_id = TouchpadScrollScreenView::kScreenId;
  summary.icon_id = "oobe-40:scroll-choobe";
  summary.title_id = "choobeTouchpadScrollTitle";
  summary.is_revisitable = true;
  summary.is_synced =
      !pref_service.FindPreference(prefs::kNaturalScroll)->IsDefaultValue();
  if (summary.is_synced ||
      (WizardController::default_controller()
           ->choobe_flow_controller()
           ->IsScreenCompleted(TouchpadScrollScreenView::kScreenId))) {
    summary.subtitle_resource = RetrieveChoobeSubtitle(pref_service);
  }

  return summary;
}

}  // namespace ash
