// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quickstart_controller.h"

#include "ash/constants/ash_features.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"

namespace ash::quick_start {

namespace {

absl::optional<QuickStartController::EntryPoint> EntryPointFromScreen(
    OobeScreenId screen) {
  if (screen.name == WelcomeScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::WELCOME_SCREEN;
  } else if (screen.name == NetworkScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::NETWORK_SCREEN;
  } else if (screen.name == GaiaScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::GAIA_SCREEN;
  }
  return absl::nullopt;
}

}  // namespace

QuickStartController::QuickStartController() {
  if (features::IsOobeQuickStartEnabled()) {
    InitTargetDeviceBootstrapController();
  }
}

QuickStartController::~QuickStartController() {
  if (bootstrap_controller_) {
    bootstrap_controller_->RemoveObserver(this);
  }
}

void QuickStartController::AttachFrontend(
    QuickStartController::UiDelegate* delegate) {
  CHECK(ui_delegates_.empty()) << "Only one UI delegate shall be attached!";
  ui_delegates_.AddObserver(delegate);
}

void QuickStartController::DetachFrontend(
    QuickStartController::UiDelegate* delegate) {
  ui_delegates_.RemoveObserver(delegate);
}

void QuickStartController::UpdateUiState(UiState ui_state) {
  ui_state_ = ui_state;
  CHECK(!ui_delegates_.empty());
  for (auto& delegate : ui_delegates_) {
    delegate.OnUiUpdateRequested(ui_state_.value());
  }
}

void QuickStartController::ForceEnableQuickStart() {
  if (bootstrap_controller_) {
    return;
  }

  InitTargetDeviceBootstrapController();
}

void QuickStartController::DetermineEntryPointVisibility(
    EntryPointButtonVisibilityCallback callback) {
  // Bootstrap controller is only instantiated when the feature is enabled (also
  // via the keyboard shortcut. See |ForceEnableQuickStart|.)
  if (!bootstrap_controller_) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  // If the flow is ongoing, entry points are hidden.
  if (IsSetupOngoing()) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  bootstrap_controller_->GetFeatureSupportStatusAsync(
      base::BindOnce(&QuickStartController::OnGetQuickStartFeatureSupportStatus,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void QuickStartController::HandleFlowCancellationRequest() {
  CHECK(bootstrap_controller_);
  bootstrap_controller_->CloseOpenConnections();
  bootstrap_controller_->StopAdvertising();
  ResetState();
}

QuickStartController::EntryPoint QuickStartController::GetExitPoint() {
  return entry_point_.value();
}

void QuickStartController::InitTargetDeviceBootstrapController() {
  CHECK(LoginDisplayHost::default_host());
  CHECK(!bootstrap_controller_);

  StartObservingScreenTransitions();
  LoginDisplayHost::default_host()->GetWizardContext()->quick_start_enabled =
      true;
  bootstrap_controller_ =
      LoginDisplayHost::default_host()->GetQuickStartBootstrapController();

  // Start observing and determine the discoverable name.
  bootstrap_controller_->AddObserver(this);
  discoverable_name_ = bootstrap_controller_->GetDiscoverableName();
}

void QuickStartController::OnGetQuickStartFeatureSupportStatus(
    EntryPointButtonVisibilityCallback set_button_visibility_callback,
    TargetDeviceConnectionBroker::FeatureSupportStatus status) {
  const bool visible =
      status == TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported;

  // Make the entry point button visible when supported, otherwise keep hidden.
  std::move(set_button_visibility_callback).Run(visible);
}

void QuickStartController::OnStatusChanged(
    const TargetDeviceBootstrapController::Status& status) {
  using Step = TargetDeviceBootstrapController::Step;
  using ErrorCode = TargetDeviceBootstrapController::ErrorCode;

  switch (status.step) {
    case Step::ADVERTISING_WITH_QR_CODE: {
      controller_state_ = ControllerState::ADVERTISING;
      CHECK(absl::holds_alternative<QRCode::PixelData>(status.payload));
      qr_code_data_ = absl::get<QRCode::PixelData>(status.payload);
      UpdateUiState(UiState::SHOWING_QR);
      quick_start_metrics::RecordScreenOpened(
          quick_start_metrics::ScreenName::kSetUpAndroidPhone);
      return;
    }
    case Step::PIN_VERIFICATION: {
      CHECK(status.pin.length() == 4);
      pin_ = status.pin;
      UpdateUiState(UiState::SHOWING_PIN);
      quick_start::quick_start_metrics::RecordScreenOpened(
          quick_start_metrics::ScreenName::kSetUpAndroidPhone);
      return;
    }
    case Step::ERROR:
      NOTIMPLEMENTED();
      return;
    case Step::CONNECTING_TO_WIFI:
      UpdateUiState(UiState::CONNECTING_TO_WIFI);
      quick_start::quick_start_metrics::RecordScreenOpened(
          quick_start_metrics::ScreenName::kConnectingToWifi);
      return;
    case Step::CONNECTED_TO_WIFI:
      wifi_name_ = status.ssid;
      UpdateUiState(UiState::CONNECTED_TO_WIFI_DEBUG);
      // TODO(b:283965994) - Replace with better logic.
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->quick_start_setup_ongoing = true;
      return;
    case Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      // Intermediate state. Nothing to do.
      CHECK(controller_state_ ==
            ControllerState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      // TODO(b/298042953): Record Gaia Transfer screen shown once UI is
      // implemented.
      return;
    case Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      CHECK(controller_state_ ==
            ControllerState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      if (absl::holds_alternative<FidoAssertionInfo>(status.payload)) {
        QS_LOG(INFO) << "Successfully received FIDO assertion.";
        fido_ = absl::get<FidoAssertionInfo>(status.payload);
        UpdateUiState(UiState::SHOWING_FIDO);
        SavePhoneInstanceID();
      } else {
        CHECK(absl::holds_alternative<ErrorCode>(status.payload));
        QS_LOG(ERROR) << "Error receiving FIDO assertion. Error Code = "
                      << static_cast<int>(absl::get<ErrorCode>(status.payload));

        // TODO(b:286873060) - Implement retry mechanism/graceful exit.
        NOTIMPLEMENTED();
      }
      return;
    case Step::NONE:
    case Step::CONNECTED:
      controller_state_ = ControllerState::CONNECTED;
      return;
    case Step::ADVERTISING_WITHOUT_QR_CODE:
      // TODO(b/282934168): Implement these screens fully
      QS_LOG(INFO) << "Hit screen which is not implemented. Continuing";
      return;
  }
}

void QuickStartController::OnCurrentScreenChanged(OobeScreenId previous_screen,
                                                  OobeScreenId current_screen) {
  current_screen_ = current_screen;
  previous_screen_ = previous_screen;

  // Just switched into the quick start screen.
  if (current_screen_ == QuickStartScreenHandler::kScreenId) {
    HandleTransitionToQuickStartScreen();
  }
}

void QuickStartController::OnDestroyingOobeUI() {
  observation_.Reset();
}

void QuickStartController::StartObservingScreenTransitions() {
  CHECK(LoginDisplayHost::default_host());
  CHECK(LoginDisplayHost::default_host()->GetOobeUI());
  observation_.Observe(LoginDisplayHost::default_host()->GetOobeUI());
}

void QuickStartController::HandleTransitionToQuickStartScreen() {
  CHECK(current_screen_ == QuickStartScreenHandler::kScreenId);

  // No ongoing setup. Entering the screen via entry point.
  if (!IsSetupOngoing()) {
    // Start by setting the UI to show a loading spinner with a cancel button.
    CHECK(!ui_state_.has_value()) << "Found UI state without ongoing setup!";
    UpdateUiState(UiState::LOADING);

    // Request advertising to start.
    controller_state_ = ControllerState::INITIALIZING;
    bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
    CHECK(!entry_point_.has_value()) << "Entry point without ongoing setup";

    // Keep track of where the flow originated.
    const auto entry_point = EntryPointFromScreen(previous_screen_.value());
    CHECK(entry_point.has_value()) << "Unknown entry point!";
    entry_point_ = entry_point;
  } else {
    // The flow must be resuming after reaching the UserCreation screen. Note
    // the the UserCreationScreen is technically never shown when it switches
    // to QuickStart, so |previous_screen_| is one of the many screens that
    // may have appeared up to this point.
    // TODO(b:283965994) - Imrpve the resume logic.
    CHECK(controller_state_ == ControllerState::CONNECTED);
    controller_state_ = ControllerState::CONTINUING_AFTER_ENROLLMENT_CHECKS;

    bootstrap_controller_->AttemptGoogleAccountTransfer();
    UpdateUiState(UiState::TRANSFERRING_GAIA_CREDENTIALS);
  }
}

void QuickStartController::SavePhoneInstanceID() {
  DCHECK(bootstrap_controller_);
  std::string phone_instance_id = bootstrap_controller_->GetPhoneInstanceId();
  if (phone_instance_id.empty()) {
    return;
  }

  QS_LOG(INFO) << "Adding Phone Instance ID to Wizard Object for Unified "
                  "Setup UI enhancements. quick_start_phone_instance_id: "
               << phone_instance_id;
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_phone_instance_id = phone_instance_id;
}

void QuickStartController::ResetState() {
  entry_point_.reset();
  qr_code_data_.reset();
  pin_.reset();
  fido_.reset();
  wifi_name_.reset();
  controller_state_ = ControllerState::NOT_ACTIVE;
  ui_state_.reset();
}

}  // namespace ash::quick_start
