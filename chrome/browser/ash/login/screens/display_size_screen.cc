// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/display_size_screen.h"

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"
#include "ui/display/display.h"
#include "ui/display/manager/display_manager.h"
#include "ui/display/manager/util/display_manager_util.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";

std::vector<float> GetZoomFactors() {
  const auto display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const auto& info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
  auto factors = display::GetDisplayZoomFactors(info.display_modes()[0]);
  return factors;
}

float GetCurrentZoomFactor() {
  const auto display_id =
      display::Screen::GetScreen()->GetPrimaryDisplay().id();
  const auto& info =
      ash::Shell::Get()->display_manager()->GetDisplayInfo(display_id);
  return info.zoom_factor();
}

}  // namespace

// static
std::string DisplaySizeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::kNext:
      return "Next";
    case Result::kNotApplicable:
      return BaseScreen::kNotApplicable;
  }
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

  if (features::IsOobeChoobeEnabled()) {
    auto* choobe_controller =
        WizardController::default_controller()->choobe_flow_controller();
    if (choobe_controller) {
      return choobe_controller->ShouldScreenBeSkipped(
          DisplaySizeScreenView::kScreenId);
    }
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
  data.Set("currentSize", GetCurrentZoomFactor());
  view_->Show(std::move(data));
}

void DisplaySizeScreen::HideImpl() {}

void DisplaySizeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
    CHECK_EQ(args.size(), 2u);
    double selected_factor = args[1].GetDouble();

    Profile* profile = ProfileManager::GetActiveUserProfile();
    profile->GetPrefs()->SetDouble(prefs::kOobeDisplaySizeFactorDeferred,
                                   selected_factor);
    exit_callback_.Run(Result::kNext);
    return;
  }
  BaseScreen::OnUserAction(args);
}

ScreenSummary DisplaySizeScreen::GetScreenSummary() {
  ScreenSummary summary;
  summary.screen_id = DisplaySizeScreenView::kScreenId;
  summary.icon_id = "oobe-32:display";
  summary.title_id = "choobeDisplaySizeTitle";
  summary.is_revisitable = true;
  summary.is_synced = false;
  return summary;
}

}  // namespace ash
