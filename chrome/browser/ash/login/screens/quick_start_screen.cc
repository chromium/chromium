// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/quick_start_screen.h"

#include "base/memory/weak_ptr.h"
#include "base/notreached.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/fido_assertion_info.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/qr_code.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "third_party/abseil-cpp/absl/types/variant.h"

namespace ash {

namespace {

constexpr const char kUserActionCancelClicked[] = "cancel";
constexpr const char kUserActionWifiConnected[] = "wifi_connected";

}  // namespace

// static
std::string QuickStartScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CANCEL_AND_RETURN_TO_WELCOME:
      return "CancelAndReturnToWelcome";
    case Result::CANCEL_AND_RETURN_TO_NETWORK:
      return "CancelAndReturnToNetwork";
    case Result::CANCEL_AND_RETURN_TO_SIGNIN:
      return "CancelAndReturnToSignin";
    case Result::WIFI_CONNECTED:
      return "WifiConnected";
  }
}

QuickStartScreen::QuickStartScreen(base::WeakPtr<TView> view,
                                   QuickStartController* controller,
                                   const ScreenExitCallback& exit_callback)
    : BaseScreen(QuickStartView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      controller_(controller),
      exit_callback_(exit_callback) {}

QuickStartScreen::~QuickStartScreen() {
  if (controller_->bootstrap_controller()) {
    controller_->bootstrap_controller()->RemoveObserver(this);
  }
}

bool QuickStartScreen::MaybeSkip(WizardContext& context) {
  return false;
}

void QuickStartScreen::ShowImpl() {
  if (!view_) {
    return;
  }
  view_->Show();

  DCHECK(controller_->bootstrap_controller());
  controller_->bootstrap_controller()->AddObserver(this);
  DetermineDiscoverableName();

  switch (flow_state_) {
    case FlowState::INITIAL:
      controller_->bootstrap_controller()->StartAdvertisingAndMaybeGetQRCode();
      break;
    case FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS:
      view_->ShowTransferringGaiaCredentials();
      controller_->bootstrap_controller()->AttemptGoogleAccountTransfer();
      break;
    case FlowState::RESUMING_AFTER_CRITICAL_UPDATE:
    case FlowState::UNKNOWN:
      NOTREACHED();
      break;
  }
}

void QuickStartScreen::SetFlowState(FlowState flow_state) {
  flow_state_ = flow_state;
}

void QuickStartScreen::SetEntryPoint(EntryPoint entry_point) {
  entry_point_ = entry_point;
}

void QuickStartScreen::HideImpl() {
  DCHECK(controller_->bootstrap_controller());
  controller_->bootstrap_controller()->RemoveObserver(this);
}

void QuickStartScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionCancelClicked) {
    DCHECK(controller_->bootstrap_controller());
    controller_->bootstrap_controller()->CloseOpenConnections();
    controller_->bootstrap_controller()->StopAdvertising();
    switch (entry_point_) {
      case EntryPoint::WELCOME_SCREEN:
        exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_WELCOME);
        return;
      case EntryPoint::NETWORK_SCREEN:
        exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_NETWORK);
        return;
      case EntryPoint::SIGNIN_SCREEN:
        exit_callback_.Run(Result::CANCEL_AND_RETURN_TO_SIGNIN);
        return;
    }
  } else if (action_id == kUserActionWifiConnected) {
    exit_callback_.Run(Result::WIFI_CONNECTED);
  }
}

void QuickStartScreen::OnStatusChanged(
    const quick_start::TargetDeviceBootstrapController::Status& status) {
  using Step = quick_start::TargetDeviceBootstrapController::Step;
  using QRCodePixelData = quick_start::QRCode::PixelData;

  switch (status.step) {
    case Step::ADVERTISING_WITH_QR_CODE: {
      CHECK(absl::holds_alternative<QRCodePixelData>(status.payload));
      if (!view_) {
        return;
      }
      const auto& code = absl::get<QRCodePixelData>(status.payload);
      base::Value::List qr_code_list;
      for (const auto& it : code) {
        qr_code_list.Append(base::Value(static_cast<bool>(it & 1)));
      }
      view_->SetQRCode(std::move(qr_code_list));
      return;
    }
    case Step::PIN_VERIFICATION: {
      CHECK(status.pin.length() == 4);
      view_->SetPIN(status.pin);
      return;
    }
    case Step::GAIA_CREDENTIALS: {
      SavePhoneInstanceID();
      return;
    }
    case Step::ERROR:
      NOTIMPLEMENTED();
      return;
    case Step::CONNECTING_TO_WIFI:
      view_->ShowConnectingToWifi();
      return;
    case Step::CONNECTED_TO_WIFI:
      view_->ShowConnectedToWifi(status.ssid, status.password);
      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->quick_start_setup_ongoing = true;
      return;

    case Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      // Intermediate state. Nothing to do.
      CHECK(flow_state_ == FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      break;
    case Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      CHECK(flow_state_ == FlowState::CONTINUING_AFTER_ENROLLMENT_CHECKS);
      OnTransferredGoogleAccountDetails(status);
      break;
    case Step::NONE:
    case Step::ADVERTISING_WITHOUT_QR_CODE:
    case Step::CONNECTED:
      // TODO(b/282934168): Implement these screens fully
      quick_start::QS_LOG(INFO)
          << "Hit screen which is not implemented. Continuing";
      return;
  }
}

void QuickStartScreen::OnTransferredGoogleAccountDetails(
    const quick_start::TargetDeviceBootstrapController::Status& status) {
  using FidoAssertionInfo = quick_start::FidoAssertionInfo;
  using ErrorCode = quick_start::TargetDeviceBootstrapController::ErrorCode;

  if (absl::holds_alternative<FidoAssertionInfo>(status.payload)) {
    quick_start::QS_LOG(INFO) << "Successfully received FIDO assertion.";
    auto fido_assertion = absl::get<FidoAssertionInfo>(status.payload);
    view_->ShowFidoAssertionReceived(fido_assertion.email);
  } else {
    CHECK(absl::holds_alternative<ErrorCode>(status.payload));
    quick_start::QS_LOG(ERROR)
        << "Error receiving FIDO assertion. Error Code = "
        << static_cast<int>(absl::get<ErrorCode>(status.payload));

    // TODO(b:286873060) - Implement retry mechanism/graceful exit.
    NOTIMPLEMENTED();
  }
}

void QuickStartScreen::DetermineDiscoverableName() {
  DCHECK(controller_->bootstrap_controller());
  discoverable_name_ =
      controller_->bootstrap_controller()->GetDiscoverableName();
  if (view_) {
    view_->SetDiscoverableName(discoverable_name_);
  }
}

void QuickStartScreen::SavePhoneInstanceID() {
  DCHECK(controller_->bootstrap_controller());
  std::string phone_instance_id =
      controller_->bootstrap_controller()->GetPhoneInstanceId();
  if (phone_instance_id.empty()) {
    return;
  }

  quick_start::QS_LOG(INFO)
      << "Adding Phone Instance ID to Wizard Object for Unified "
         "Setup UI enhancements. quick_start_phone_instance_id: "
      << phone_instance_id;
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_phone_instance_id = phone_instance_id;
}

}  // namespace ash
