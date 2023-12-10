// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"

namespace ash {

namespace {

constexpr const char kUserActionCancelClicked[] = "cancel";

base::Value::List ConvertQrCode(quick_start::QRCode::PixelData qr_code) {
  base::Value::List qr_code_list;
  for (const auto& it : qr_code) {
    qr_code_list.Append(base::Value(static_cast<bool>(it & 1)));
  }
  return qr_code_list;
}

}  // namespace

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL_AND_RETURN_TO_WELCOME:
      return "CancelAndReturnToWelcome";
    case Result::CANCEL_AND_RETURN_TO_NETWORK:
      return "CancelAndReturnToNetwork";
    case Result::CANCEL_AND_RETURN_TO_GAIA_INFO:
      return "CancelAndReturnToGaiaInfo";
    case Result::CANCEL_AND_RETURN_TO_SIGNIN:
      return "CancelAndReturnToSignin";
    case Result::WIFI_CREDENTIALS_RECEIVED:
      return "WifiCredentialsReceived";
  }
}

QuickStartScreen::QuickStartScreen(
    base::WeakPtr<TView> view,
    quick_start::QuickStartController* controller,
    const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      controller_(controller),
      exit_callback_(exit_callback) {}

QuickStartScreen::~QuickStartScreen() {
  if (controller_) {
    controller_->DetachFrontend(this);
  }
}

bool QuickStartScreen::MaybeSkip(WizardContext& context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  // Attach to the controller whenever the screen is shown.
  // QuickStartController will request the UI updates via |OnUiUpdateRequested|.
  controller_->AttachFrontend(this);

  if (!view_) {
    return;
  }
  view_->Show();
}

void QuickStartScreen::HideImpl() {
  // Detach from the controller whenever the screen is hidden.
  controller_->DetachFrontend(this);
}

void QuickStartScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelClicked) {
    controller_->DetachFrontend(this);
    controller_->AbortFlow(quick_start::QuickStartController::AbortFlowReason::
                               USER_CLICKED_CANCEL);
    ExitScreen();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void QuickStartScreen::OnUiUpdateRequested(
    quick_start::QuickStartController::UiState state) {
  if (!view_) {
    return;
  }

  // Update discoverable name
  view_->SetDiscoverableName(controller_->GetDiscoverableName());

  switch (state) {
    case ash::quick_start::QuickStartController::UiState::SHOWING_QR:
      view_->SetQRCode(ConvertQrCode(controller_->GetQrCode()));
      break;
    case quick_start::QuickStartController::UiState::SHOWING_FIDO:
      view_->ShowFidoAssertionReceived(controller_->GetFidoAssertion().email);
      break;
    case quick_start::QuickStartController::UiState::SHOWING_PIN:
      view_->SetPIN(controller_->GetPin());
      break;
    case quick_start::QuickStartController::UiState::CONNECTING_TO_WIFI:
      view_->ShowConnectingToWifi();
      break;
    case quick_start::QuickStartController::UiState::WIFI_CREDENTIALS_RECEIVED:
      exit_callback_.Run(Result::WIFI_CREDENTIALS_RECEIVED);
      break;
    case ash::quick_start::QuickStartController::UiState::
        TRANSFERRING_GAIA_CREDENTIALS:
      view_->ShowTransferringGaiaCredentials();
      break;
    case ash::quick_start::QuickStartController::UiState::LOADING:
      // TODO(b:283724988) - Add method to view to show the loading spinner.
      break;
    case ash::quick_start::QuickStartController::UiState::EXIT_SCREEN:
      // Controller requested the flow to be aborted.
      controller_->DetachFrontend(this);
      ExitScreen();
  }
}

void QuickStartScreen::ExitScreen() {
  // Get exit point before cancelling the whole flow.
  const auto return_entry_point = controller_->GetExitPoint();
  switch (return_entry_point) {
    case ash::quick_start::QuickStartController::EntryPoint::WELCOME_SCREEN:
      exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_WELCOME);
      return;
    case ash::quick_start::QuickStartController::EntryPoint::NETWORK_SCREEN:
      exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_NETWORK);
      return;
    case ash::quick_start::QuickStartController::EntryPoint::GAIA_INFO_SCREEN:
      exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_GAIA_INFO);
      return;
    case ash::quick_start::QuickStartController::EntryPoint::GAIA_SCREEN:
      exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_SIGNIN);
      return;
  }
}

}  // namespace ash
