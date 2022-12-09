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

// to check with the ChoobeFlowController whether to skip
bool ChoobeScreen::MaybeSkip(WizardContext& context) {
  auto* choobe_controller_ =
      WizardController::default_controller()->GetChoobeFlowController();
  if (choobe_controller_ && choobe_controller_->IsChoobeFlowActive())
    return false;

  exit_callback_.Run(Result::NOT_APPLICABLE);
  return true;
}

void ChoobeScreen::SkipScreen() {
  WizardController::default_controller()->GetChoobeFlowController()->Stop(
      *ProfileManager::GetActiveUserProfile()->GetPrefs());
  exit_callback_.Run(Result::SKIPPED);
}

void ChoobeScreen::OnSelect(base::Value::List screens) {
  WizardController::default_controller()
      ->GetChoobeFlowController()
      ->OnScreensSelected(*ProfileManager::GetActiveUserProfile()->GetPrefs(),
                          std::move(screens));
  exit_callback_.Run(Result::SELECTED);
}

void ChoobeScreen::ShowImpl() {
  if (!view_)
    return;

  auto screens = WizardController::default_controller()
                     ->GetChoobeFlowController()
                     ->GetEligibleCHOOBEScreens();

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
