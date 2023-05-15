// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/display_size_screen.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/display_size_screen_handler.h"

namespace ash {
namespace {

constexpr const char kUserActionNext[] = "next";

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

  view_->Show();
}

void DisplaySizeScreen::HideImpl() {}

void DisplaySizeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionNext) {
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
