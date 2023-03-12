// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/choobe_screen.h"
#include "chrome/browser/profiles/profile_manager.h"

#include "chrome/browser/ash/login/choobe_flow_controller.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/choobe_screen_handler.h"

namespace ash {
namespace {

constexpr const char kUserActionSkip[] = "choobeSkip";
constexpr const char kUserActionSelect[] = "choobeSelect";

}  // namespace

// static
std::string ChoobeScreen::GetResultString(Result result) {
  switch (result) {
    case Result::SELECTED:
      return "Selected";
    case Result::SKIPPED:
      return "Skipped";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

ChoobeScreen::ChoobeScreen(base::WeakPtr<ChoobeScreenView> view,
                           const ScreenExitCallback& exit_callback)
    : BaseScreen(ChoobeScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

ChoobeScreen::~ChoobeScreen() = default;

// to check with the ChoobeFlowController whether to skip CHOOBE screen.
bool ChoobeScreen::MaybeSkip(WizardContext& context) {
  if (context.skip_post_login_screens_for_tests) {
    return true;
  }

  if (WizardController::default_controller()->choobe_flow_controller()) {
    return false;
  }

  if (ChoobeFlowController::ShouldStartChoobe()) {
    return false;
  }

  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void ChoobeScreen::SkipScreen() {
  exit_callback_.Run(Result::SKIPPED);
}

void ChoobeScreen::OnSelect(base::Value::List screens) {
  WizardController::default_controller()
      ->choobe_flow_controller()
      ->OnScreensSelected(*ProfileManager::GetActiveUserProfile()->GetPrefs(),
                          std::move(screens));
  exit_callback_.Run(Result::SELECTED);
}

void ChoobeScreen::ShowImpl() {
  if (!view_)
    return;

  auto* controller = WizardController::default_controller();
  if (!controller->choobe_flow_controller()) {
    controller->CreateChoobeFlowController();
  }

  auto screens =
      controller->choobe_flow_controller()->GetEligibleScreensSummaries();

  view_->Show(std::move(screens));
}

void ChoobeScreen::HideImpl() {}

void ChoobeScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionSkip) {
    SkipScreen();
    return;
  }

  if (action_id == kUserActionSelect) {
    CHECK_EQ(args.size(), 2u);
    OnSelect(args[1].GetList().Clone());
    return;
  }

  BaseScreen::OnUserAction(args);
}

}  // namespace ash
