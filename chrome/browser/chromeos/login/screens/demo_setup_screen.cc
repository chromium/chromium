// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/screens/demo_setup_screen.h"

#include "base/bind.h"
#include "chrome/browser/chromeos/login/wizard_controller.h"
#include "chrome/browser/ui/webui/chromeos/login/demo_setup_screen_handler.h"
#include "chromeos/dbus/session_manager/session_manager_client.h"

namespace {

constexpr char kUserActionStartSetup[] = "start-setup";
constexpr char kUserActionClose[] = "close-setup";
constexpr char kUserActionPowerwash[] = "powerwash";

}  // namespace

namespace chromeos {

DemoSetupScreen::DemoSetupScreen(DemoSetupScreenView* view,
                                 const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoSetupScreenView::kScreenId),
      view_(view),
      exit_callback_(exit_callback) {
  DCHECK(view_);
  view_->Bind(this);
}

DemoSetupScreen::~DemoSetupScreen() {
  if (view_)
    view_->Bind(nullptr);
}

void DemoSetupScreen::Show() {
  if (view_)
    view_->Show();
}

void DemoSetupScreen::Hide() {
  if (view_)
    view_->Hide();
}

void DemoSetupScreen::OnUserAction(const std::string& action_id) {
  if (action_id == kUserActionStartSetup) {
    StartEnrollment();
  } else if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::CANCELED);
  } else if (action_id == kUserActionPowerwash) {
    SessionManagerClient::Get()->StartDeviceWipe();
  } else {
    BaseScreen::OnUserAction(action_id);
  }
}


void DemoSetupScreen::StartEnrollment() {
  // Demo setup screen is only shown in OOBE.
  DCHECK(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  DemoSetupController* demo_controller =
      WizardController::default_controller()->demo_setup_controller();
  demo_controller->Enroll(base::BindOnce(&DemoSetupScreen::OnSetupSuccess,
                                         weak_ptr_factory_.GetWeakPtr()),
                          base::BindOnce(&DemoSetupScreen::OnSetupError,
                                         weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupScreen::OnSetupError(
    const DemoSetupController::DemoSetupError& error) {
  view_->OnSetupFailed(error);
}

void DemoSetupScreen::OnSetupSuccess() {
  exit_callback_.Run(Result::COMPLETED);
}

void DemoSetupScreen::OnViewDestroyed(DemoSetupScreenView* view) {
  if (view_ == view)
    view_ = nullptr;
}

}  // namespace chromeos
