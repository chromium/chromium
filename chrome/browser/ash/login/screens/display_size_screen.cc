// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/display_size_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_pref_names.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/ranges.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/users/chrome_user_manager_util.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "chromeos/ash/components/install_attributes/install_attributes.h"
#include "chromeos/ash/components/settings/cros_settings.h"
#include "chromeos/ash/components/settings/cros_settings_names.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_util.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";
constexpr const char kUserActionReturn[] = "return";

std::vector<float> GetZoomFactors() {
  const auto display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const auto& info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
  auto factors = display::GetDisplayZoomFactors(info.display_modes()[0]);
  return factors;
}

float GetCurrentZoomFactor(PrefService* prefs) {
  if (!prefs->FindPreference(prefs::kOobeDisplaySizeFactorDeferred)
           ->IsDefaultValue()) {
    return prefs->GetDouble(prefs::kOobeDisplaySizeFactorDeferred);
  }

  const auto display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const auto& info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
  return info.zoom_factor();
}

std::string RetrieveChoobeSubtitle(PrefService* prefs) {
  int percentage = std::round(GetCurrentZoomFactor(prefs) * 100);
  return base::NumberToString(percentage);
}

bool ShouldShowChoobeReturnButton(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return false;
  }
  return controller->ShouldShowReturnButton(DisplaySizeScreenView::kScreenId);
}

void PersistSelectedFactor(PrefService* prefs, double factor) {
  float current_factor = GetCurrentZoomFactor(prefs);
  bool factor_changed = !base::IsApproximatelyEqual(
      current_factor, static_cast<float>(factor), 0.01f);
  base::UmaHistogramBoolean("OOBE.CHOOBE.SettingChanged.Display-size",
                            factor_changed);

  prefs->SetDouble(prefs::kOobeDisplaySizeFactorDeferred, factor);
}

void ReportScreenCompletedToChoobe(ChoobeFlowController* controller) {
  if (!features::IsOobeChoobeEnabled() || !controller) {
    return;
  }
  controller->OnScreenCompleted(
      *ProfileManager::GetActiveUserProfile()->GetPrefs(),
      DisplaySizeScreenView::kScreenId);
}

}  // namespace

// static
std::string DisplaySizeScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

void DisplaySizeScreen::MaybeUpdateZoomFactor(Profile* profile) {
  auto* prefs = profile->GetPrefs();
  if (!prefs->HasPrefPath(prefs::kOobeDisplaySizeFactorDeferred)) {
    return;
  }

  auto factors = GetZoomFactors();
  // Verify thae existence of available factors.
  if (factors.empty()) {
    return;
  }

  double stored_zoom_factor =
      prefs->GetDouble(prefs::kOobeDisplaySizeFactorDeferred);
  prefs->ClearPref(prefs::kOobeDisplaySizeFactorDeferred);

  // Find the nearest available zoom factor to the stored zoom factor. This is
  // done to account for any changes in the available zoom factors since the
  // preference was stored.
  float selected_zoom_factor = factors[0];
  for (float factor : factors) {
    if (abs(stored_zoom_factor - factor) < abs(selected_zoom_factor - factor)) {
      selected_zoom_factor = factor;
    }
  }

  auto display_id_ = display::Screen::GetScreen()->GetPrimaryDisplay().id();
  display::DisplayManager* display_manager = Shell::Get()->display_manager();
  display_manager->UpdateZoomFactor(display_id_, selected_zoom_factor);
}

DisplaySizeScreen::DisplaySizeScreen(base::WeakPtr<DisplaySizeScreenView> view,
                                     const ScreenExitCallback& exit_callback)
    : BaseScreen(DisplaySizeScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DisplaySizeScreen::~DisplaySizeScreen() = default;

bool DisplaySizeScreen::ShouldBeSkipped(const WizardContext& context) const {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  if (chrome_user_manager_util::IsManagedGuestSessionOrEphemeralLogin()) {
    return true;
  }

  if (features::IsOobeChoobeEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller && choobe_controller->ShouldScreenBeSkipped(
                                 DisplaySizeScreenView::kScreenId)) {
      return true;
    }
  }

  // Skip the screen if the `recommended` value in `DeviceDisplayResolution`
  // policy is set to false.
  if (ash::InstallAttributes::Get()->IsEnterpriseManaged()) {
    const base::Value::Dict* resolution_pref = nullptr;
    ash::CrosSettings::Get()->GetDictionary(ash::kDeviceDisplayResolution,
                                            &resolution_pref);
    if (resolution_pref && !resolution_pref->empty()) {
      const std::optional<bool> recommended_value = resolution_pref->FindBool(
          ash::kDeviceDisplayResolutionKeyRecommended);
      if (!recommended_value.value_or(false)) {
        return true;
      }
    }
  }

  // Skip the screen if `ShowDisplaySizeScreenEnabled` preference is set by
  // admin to false.
  const PrefService::Preference* pref =
      ProfileManager::GetActiveUserProfile()->GetPrefs()->FindPreference(
          prefs::kShowDisplaySizeScreenEnabled);
  if (pref->IsManaged() && !pref->GetValue()->GetBool()) {
    return true;
  }

  return false;
}

bool DisplaySizeScreen::MaybeSkip(WizardContext& context) {
  if (!ShouldBeSkipped(context)) {
    return false;
  }

  exit_callback_.Run(Result::kNotApplicable);
  return true;
}

void DisplaySizeScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  auto factors = GetZoomFactors();
  base::Value::List factors_list;
  for (auto factor : factors) {
    factors_list.Append(base::Value(factor));
  }

  base::Value::Dict data;
  data.Set("availableSizes", std::move(factors_list));
  data.Set(
      "currentSize",
      GetCurrentZoomFactor(ProfileManager::GetActiveUserProfile()->GetPrefs()));
  data.Set(
      "shouldShowReturn",
      ShouldShowChoobeReturnButton(
          WizardController::default_controller()->choobe_flow_controller()));
  view_->Show(std::move(data));
}

void DisplaySizeScreen::HideImpl() {}

void DisplaySizeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    PersistSelectedFactor(ProfileManager::GetActiveUserProfile()->GetPrefs(),
                          args[1].GetDouble());
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller());
    exit_callback_.Run(Result::kNext);
    return;
  }

  if (action_id == kUserActionReturn) {
    CHECK_EQ(args.size(), 2u);
    PersistSelectedFactor(ProfileManager::GetActiveUserProfile()->GetPrefs(),
                          args[1].GetDouble());
    ReportScreenCompletedToChoobe(
        WizardController::default_controller()->choobe_flow_controller());
    context()->return_to_choobe_screen = true;
    exit_callback_.Run(Result::kNext);
    return;
  }

  BaseScreen::OnUserAction(args);
}

ScreenSummary DisplaySizeScreen::GetScreenSummary() {
  ScreenSummary summary;
  summary.screen_id = DisplaySizeScreenView::kScreenId;
  summary.icon_id = "oobe-40:display-size-choobe";
  summary.title_id = "choobeDisplaySizeTitle";
  summary.is_revisitable = true;
  summary.is_synced = false;

  if (WizardController::default_controller()
          ->choobe_flow_controller()
          ->IsScreenCompleted(DisplaySizeScreenView::kScreenId)) {
    summary.subtitle_resource = RetrieveChoobeSubtitle(
        ProfileManager::GetActiveUserProfile()->GetPrefs());
  }

  return summary;
}

}  // namespace ash
