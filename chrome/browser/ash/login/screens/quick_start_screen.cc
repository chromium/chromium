// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include "base/memory/weak_ptr.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"

namespace ash {

namespace {

constexpr const char kUserActionCancelClicked[] = "cancel";
constexpr const char kUserActionNextClicked[] = "next";
constexpr const char kUserActionTurnOnBluetooth[] = "turn_on_bluetooth";

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
  // LINT.IfChange(UsageMetrics)
  switch (result) {
    case Result::CANCEL_AND_RETURN_TO_WELCOME:
      return "CancelAndReturnToWelcome";
    case Result::CANCEL_AND_RETURN_TO_NETWORK:
      return "CancelAndReturnToNetwork";
    case Result::CANCEL_AND_RETURN_TO_GAIA_INFO:
      return "CancelAndReturnToGaiaInfo";
    case Result::CANCEL_AND_RETURN_TO_SIGNIN:
      return "CancelAndReturnToSignin";
    case Result::SETUP_COMPLETE_NEXT_BUTTON:
      return "SetupCompleteNextButton";
    case Result::WIFI_CREDENTIALS_RECEIVED:
      return "WifiCredentialsReceived";
    case Result::FALLBACK_URL_ON_GAIA:
      return "FallbackUrlOnGaia";
  }
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
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

  // Show the initial UI step which is just blank. The controller
  // then updates the UI via the local observer 'OnUiUpdateRequested'.
  view_->ShowInitialUiStep();
  view_->Show();
}

void QuickStartScreen::HideImpl() {
  // Detach from the controller whenever the screen is hidden.
  controller_->DetachFrontend(this);
  session_refresher_.reset();
}

void QuickStartScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelClicked) {
    controller_->DetachFrontend(this);
    controller_->AbortFlow(quick_start::QuickStartController::AbortFlowReason::
                               USER_CLICKED_CANCEL);
    ExitScreen();
  } else if (action_id == kUserActionNextClicked) {
    controller_->DetachFrontend(this);
    exit_callback_.Run(Result::SETUP_COMPLETE_NEXT_BUTTON);
  } else if (action_id == kUserActionTurnOnBluetooth) {
    controller_->OnBluetoothPermissionGranted();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

void QuickStartScreen::OnUiUpdateRequested(
    quick_start::QuickStartController::UiState state) {
  if (!view_) {
    return;
  }

  switch (state) {
    case ash::quick_start::QuickStartController::UiState::SHOWING_QR:
      view_->SetWillRequestWiFi(controller_->WillRequestWiFi());
      view_->SetQRCode(ConvertQrCode(controller_->GetQrCode().GetPixelData()),
                       controller_->GetQrCode().GetQRCodeURLString());
      break;
    case quick_start::QuickStartController::UiState::SHOWING_PIN:
      view_->SetWillRequestWiFi(controller_->WillRequestWiFi());
      view_->SetPIN(controller_->GetPin());
      break;
    case quick_start::QuickStartController::UiState::CONNECTING_TO_WIFI:
      view_->ShowConnectingToWifi();
      break;
    case quick_start::QuickStartController::UiState::WIFI_CREDENTIALS_RECEIVED:
      exit_callback_.Run(Result::WIFI_CREDENTIALS_RECEIVED);
      break;
    case quick_start::QuickStartController::UiState::CONFIRM_GOOGLE_ACCOUNT:
      view_->ShowConfirmGoogleAccount();
      break;
    case ash::quick_start::QuickStartController::UiState::SIGNING_IN:
      view_->ShowSigningInStep();
      view_->SetUserEmail(controller_->GetUserInfo().email);
      view_->SetUserFullName(controller_->GetUserInfo().full_name);
      view_->SetUserAvatar(controller_->GetUserInfo().avatar_url);
      break;
    case ash::quick_start::QuickStartController::UiState::CREATING_ACCOUNT:
      view_->ShowCreatingAccountStep();
      break;
    case ash::quick_start::QuickStartController::UiState::FALLBACK_URL_FLOW:
      // WizardController will handle this edge case and populate the URL.
      exit_callback_.Run(Result::FALLBACK_URL_ON_GAIA);
      break;
    case ash::quick_start::QuickStartController::UiState::SETUP_COMPLETE:
      // Keep Cryptohome's AuthSession alive while on the setup complete step.
      if (context()->extra_factors_token) {
        session_refresher_ = AuthSessionStorage::Get()->KeepAlive(
            context()->extra_factors_token.value());
      }
      view_->ShowSetupCompleteStep(controller_->did_transfer_wifi());
      break;
    case ash::quick_start::QuickStartController::UiState::CONNECTING_TO_PHONE:
      view_->ShowConnectingToPhoneStep();
      break;
    case ash::quick_start::QuickStartController::UiState::
        SHOWING_BLUETOOTH_DIALOG:
      view_->SetWillRequestWiFi(controller_->WillRequestWiFi());
      view_->ShowBluetoothDialog();
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
