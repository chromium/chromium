// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/demo_setup_screen.h"

#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/demo_setup_screen_handler.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"

namespace {

constexpr char kUserActionStartSetup[] = "start-setup";
constexpr char kUserActionClose[] = "close-setup";
constexpr char kUserActionPowerwash[] = "powerwash";

}  // namespace

namespace ash {

// static
std::string DemoSetupScreen::GetResultString(Result result) {
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::kCompleted:
      return "Completed";
    case Result::kCanceled:
      return "Canceled";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

DemoSetupScreen::DemoSetupScreen(base::WeakPtr<DemoSetupScreenView> view,
                                 const ScreenExitCallback& exit_callback)
    : BaseScreen(DemoSetupScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback) {}

DemoSetupScreen::~DemoSetupScreen() = default;

void DemoSetupScreen::ShowImpl() {
  if (view_)
    view_->Show();
}

void DemoSetupScreen::HideImpl() {}

void DemoSetupScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionStartSetup) {
    StartEnrollment();
  } else if (action_id == kUserActionClose) {
    exit_callback_.Run(Result::kCanceled);
  } else if (action_id == kUserActionPowerwash) {
    SessionManagerClient::Get()->StartDeviceWipe(base::DoNothing());
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void DemoSetupScreen::StartEnrollment() {
  // Demo setup screen is only shown in OOBE.
  DCHECK(DemoSetupController::IsOobeDemoSetupFlowInProgress());
  DemoSetupController* demo_controller =
      WizardController::default_controller()->demo_setup_controller();
  demo_controller->Enroll(
      base::BindOnce(&DemoSetupScreen::OnSetupSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&DemoSetupScreen::OnSetupError,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindRepeating(&DemoSetupScreen::SetCurrentSetupStep,
                          weak_ptr_factory_.GetWeakPtr()));
}

void DemoSetupScreen::SetCurrentSetupStep(
    const DemoSetupController::DemoSetupStep current_step) {
  if (!view_)
    return;
  view_->SetCurrentSetupStep(current_step);
}

void DemoSetupScreen::SetCurrentSetupStepForTest(
    const DemoSetupController::DemoSetupStep current_step) {
  SetCurrentSetupStep(current_step);
}

void DemoSetupScreen::OnSetupError(
    const DemoSetupController::DemoSetupError& error) {
  if (!view_)
    return;
  view_->OnSetupFailed(error);
}

void DemoSetupScreen::OnSetupSuccess() {
  exit_callback_.Run(Result::kCompleted);
}

}  // namespace ash
