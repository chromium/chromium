// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quickstart_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/login/login_display_host.h"
#include "chrome/browser/ui/webui/ash/login/add_child_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/error_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/user_creation_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/welcome_screen_handler.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/network_type_pattern.h"
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
#include "chromeos/ash/components/quick_start/types.h"
#include "components/account_id/account_id.h"
#include "components/session_manager/core/session_manager.h"
#include "components/session_manager/session_manager_types.h"
#include "components/user_manager/user_type.h"

namespace ash::quick_start {

namespace {

using bluetooth_config::mojom::BluetoothDevicePropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;

std::string GetBluetoothStateString(BluetoothSystemState system_state) {
  switch (system_state) {
    case BluetoothSystemState::kDisabled:
      return "Bluetooth is turned off.";
    case BluetoothSystemState::kDisabling:
      return "Bluetooth is in the process of turning off.";
    case BluetoothSystemState::kEnabled:
      return "Bluetooth is turned on.";
    case BluetoothSystemState::kEnabling:
      return "Bluetooth is in the process of turning on.";
    case BluetoothSystemState::kUnavailable:
      return "Device does not have access to Bluetooth.";
    default:
      return "Unknown bluetooth state!";
  }
}

std::optional<QuickStartController::EntryPoint> EntryPointFromScreen(
    OobeScreenId screen) {
  if (screen.name == WelcomeScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::WELCOME_SCREEN;
  } else if (screen.name == NetworkScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::NETWORK_SCREEN;
  } else if (screen.name == GaiaInfoScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::GAIA_INFO_SCREEN;
  } else if (screen.name == GaiaScreenHandler::kScreenId.name) {
    return QuickStartController::EntryPoint::GAIA_SCREEN;
  }
  return std::nullopt;
}

QuickStartMetrics::ScreenName ScreenNameFromOobeScreenId(
    std::optional<OobeScreenId> screen_id) {
  if (!screen_id.has_value()) {
    return QuickStartMetrics::ScreenName::kNone;
  } else if (screen_id == WelcomeView::kScreenId) {
    return QuickStartMetrics::ScreenName::kWelcomeScreen;
  } else if (screen_id == NetworkScreenView::kScreenId) {
    return QuickStartMetrics::ScreenName::kNetworkScreen;
  } else if (screen_id == GaiaInfoScreenView::kScreenId) {
    return QuickStartMetrics::ScreenName::kGaiaInfoScreen;
  } else if (screen_id == GaiaView::kScreenId) {
    return QuickStartMetrics::ScreenName::kGaiaScreen;
  } else if (screen_id == UpdateView::kScreenId) {
    return QuickStartMetrics::ScreenName::
        kCheckingForUpdateAndDeterminingDeviceConfiguration;
  } else if (screen_id == UserCreationView::kScreenId) {
    return QuickStartMetrics::ScreenName::kChooseChromebookSetup;
  } else if (screen_id == ConsumerUpdateScreenView::kScreenId) {
    return QuickStartMetrics::ScreenName::kConsumerUpdate;
  } else if (screen_id == AddChildScreenView::kScreenId) {
    return QuickStartMetrics::ScreenName::kAddChild;
  } else {
    return QuickStartMetrics::ScreenName::kOther;
  }
}

QuickStartMetrics::ScreenName ScreenNameFromUiState(
    std::optional<QuickStartController::UiState> ui_state,
    QuickStartController::ControllerState controller_state) {
  if (!ui_state.has_value()) {
    return QuickStartMetrics::ScreenName::kNone;
  }

  switch (ui_state.value()) {
    case QuickStartController::UiState::SHOWING_QR:
      [[fallthrough]];
    case QuickStartController::UiState::SHOWING_PIN:
      return QuickStartMetrics::ScreenName::kQSSetUpWithAndroidPhone;
    case QuickStartController::UiState::CONNECTING_TO_WIFI:
      return QuickStartMetrics::ScreenName::kQSConnectingToWifi;
    case QuickStartController::UiState::WIFI_CREDENTIALS_RECEIVED:
      return QuickStartMetrics::ScreenName::kQSWifiCredentialsReceived;
    case QuickStartController::UiState::CONFIRM_GOOGLE_ACCOUNT:
      return QuickStartMetrics::ScreenName::kQSSelectGoogleAccount;
    case QuickStartController::UiState::SIGNING_IN:
      return QuickStartMetrics::ScreenName::kQSGettingGoogleAccountInfo;
    case QuickStartController::UiState::CREATING_ACCOUNT:
      return QuickStartMetrics::ScreenName::kQSCreatingAccount;
    case QuickStartController::UiState::SETUP_COMPLETE:
      return QuickStartMetrics::ScreenName::kQSComplete;
    case QuickStartController::UiState::FALLBACK_URL_FLOW:
      return QuickStartMetrics::ScreenName::kQSFallbackURL;
    case QuickStartController::UiState::CONNECTING_TO_PHONE:
      if (controller_state == QuickStartController::ControllerState::
                                  WAITING_TO_RESUME_AFTER_UPDATE) {
        return QuickStartMetrics::ScreenName::kQSResumingConnectionAfterUpdate;
      }
      [[fallthrough]];
    case QuickStartController::UiState::EXIT_SCREEN:
      [[fallthrough]];
    case QuickStartController::UiState::SHOWING_BLUETOOTH_DIALOG:
      [[fallthrough]];
    default:
      return QuickStartMetrics::ScreenName::kNone;
  }
}

bool IsConnectedToWiFi() {
  NetworkStateHandler* nsh = NetworkHandler::Get()->network_state_handler();
  return nsh->ConnectedNetworkByType(NetworkTypePattern::WiFi()) != nullptr;
}

TargetDeviceBootstrapController::ConnectionClosedReason
ConnectionClosedReasonFromAbortFlowReason(
    QuickStartController::AbortFlowReason reason) {
  switch (reason) {
    case QuickStartController::AbortFlowReason::USER_CLICKED_CANCEL:
      [[fallthrough]];
    case QuickStartController::AbortFlowReason::USER_CLICKED_BACK:
      [[fallthrough]];
    case QuickStartController::AbortFlowReason::SIGNIN_SCHOOL:
      [[fallthrough]];
    case QuickStartController::AbortFlowReason::ADD_CHILD:
      [[fallthrough]];
    case QuickStartController::AbortFlowReason::ENTERPRISE_ENROLLMENT:
      return TargetDeviceBootstrapController::ConnectionClosedReason::
          kUserAborted;
    case QuickStartController::AbortFlowReason::ERROR:
      return TargetDeviceBootstrapController::ConnectionClosedReason::
          kUnknownError;
  }
}

}  // namespace

QuickStartController::QuickStartController() {
  metrics_ = std::make_unique<QuickStartMetrics>();

  if (g_browser_process->local_state()->GetBoolean(
          prefs::kShouldResumeQuickStartAfterReboot)) {
    QS_LOG(INFO) << "This session should resume Quick Start after a reboot.";
    should_resume_quick_start_after_update_ = true;
    // Clear pref right away to prevent bad state in case of crash.
    g_browser_process->local_state()->ClearPref(
        prefs::kShouldResumeQuickStartAfterReboot);
  }

  // Main feature flag
  if (!features::IsOobeQuickStartEnabled()) {
    if (should_resume_quick_start_after_update_) {
      ForceEnableQuickStart();
    }
    return;
  }

  // QuickStart may not be available on the login screen.
  if (session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::OOBE &&
      !features::IsOobeQuickStartOnLoginScreenEnabled()) {
    return;
  }

  // A guest session state is SessionState::OOBE if there are no other users
  // added. Quick Start is not available in this case.
  if (ProfileManager::GetActiveUserProfile()->IsGuestSession()) {
    return;
  }

  InitTargetDeviceBootstrapController();
  StartObservingBluetoothState();
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

void QuickStartController::MaybeRecordQuickStartScreenOpened(
    QuickStartController::UiState new_ui) {
  QuickStartMetrics::ScreenName screen_name =
      ScreenNameFromUiState(new_ui, controller_state_);
  if (screen_name != QuickStartMetrics::ScreenName::kNone) {
    metrics_->QuickStartMetrics::RecordScreenOpened(screen_name);
  }
}

void QuickStartController::MaybeRecordQuickStartScreenAdvanced(
    std::optional<QuickStartController::UiState> closed_ui) {
  QuickStartMetrics::ScreenName screen_name =
      ScreenNameFromUiState(closed_ui, controller_state_);
  if (screen_name != QuickStartMetrics::ScreenName::kNone) {
    metrics_->RecordScreenClosed(
        screen_name, QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow);
  }
}

void QuickStartController::UpdateUiState(UiState ui_state) {
  QS_LOG(INFO) << "Updating UI state to " << ui_state;

  if (is_transitioning_to_quick_start_screen_) {
    is_transitioning_to_quick_start_screen_ = false;
    QuickStartMetrics::ScreenName previous_screen_name =
        ScreenNameFromOobeScreenId(previous_screen_);
    metrics_->RecordScreenClosed(
        previous_screen_name,
        QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow);
  } else {
    MaybeRecordQuickStartScreenAdvanced(ui_state_);
  }

  ui_state_ = ui_state;
  MaybeRecordQuickStartScreenOpened(ui_state);

  CHECK(!ui_delegates_.empty()) << "ui_delegates_ is empty";
  for (auto& delegate : ui_delegates_) {
    delegate.OnUiUpdateRequested(ui_state_.value());
  }
}

void QuickStartController::ForceEnableQuickStart() {
  if (bootstrap_controller_) {
    return;
  }

  InitTargetDeviceBootstrapController();
  StartObservingBluetoothState();
}

void QuickStartController::DetermineEntryPointVisibility(
    EntryPointButtonVisibilityCallback callback) {
  // Bootstrap controller is only instantiated when the feature is enabled (also
  // via the keyboard shortcut. See |ForceEnableQuickStart|.)
  if (!bootstrap_controller_) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  // QuickStart should not be enabled for Demo mode or OS Install flows
  if (DemoSetupController::IsOobeDemoSetupFlowInProgress() ||
      ash::switches::IsOsInstallAllowed()) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  // If the flow is ongoing, entry points are hidden.
  if (IsSetupOngoing()) {
    std::move(callback).Run(/*visible=*/false);
    return;
  }

  bootstrap_controller_->GetFeatureSupportStatusAsync(base::BindRepeating(
      &QuickStartController::OnGetQuickStartFeatureSupportStatus,
      weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void QuickStartController::AbortFlow(AbortFlowReason reason) {
  CHECK(bootstrap_controller_) << "Missing bootstrap_controller_";
  QuickStartMetrics::RecordAbortFlowReason(reason);

  // Screen is closed when flow aborts on these screens.
  if (current_screen_ == QuickStartScreenHandler::kScreenId ||
      current_screen_ == NetworkScreenHandler::kScreenId) {
    QuickStartMetrics::ScreenName current_screen_name =
        current_screen_ == QuickStartScreenHandler::kScreenId
            ? ScreenNameFromUiState(ui_state_, controller_state_)
            : ScreenNameFromOobeScreenId(current_screen_.value());
    metrics_->RecordScreenClosed(
        current_screen_name,
        QuickStartMetrics::MapAbortFlowReasonToScreenClosedReason(reason));
  }

  // If user proceeds with school, enterprise, or unicorn setup, allow source
  // device to gracefully close connection and show "setup complete" UI.
  constexpr AbortFlowReason kUnsupportedUserTypes[] = {
      AbortFlowReason::ENTERPRISE_ENROLLMENT, AbortFlowReason::SIGNIN_SCHOOL,
      AbortFlowReason::ADD_CHILD};
  if (base::Contains(kUnsupportedUserTypes, reason)) {
    QS_LOG(INFO) << "Aborting flow due to unsupported user type: " << reason;
    bootstrap_controller_->OnSetupComplete();
    return;
  }

  bootstrap_controller_->CloseOpenConnections(
      ConnectionClosedReasonFromAbortFlowReason(reason));
  bootstrap_controller_->StopAdvertising();
  bootstrap_controller_->Cleanup();
  ResetState();

  // Triggers a screen exit if there is a UiDelegate driving the UI.
  if (!ui_delegates_.empty()) {
    CHECK(current_screen_ == QuickStartScreenHandler::kScreenId ||
          current_screen_ == NetworkScreenHandler::kScreenId)
        << "Unexpected current_screen_.";
    ui_delegates_.begin()->OnUiUpdateRequested(UiState::EXIT_SCREEN);
  }
}

QuickStartController::EntryPoint QuickStartController::GetExitPoint() {
  return exit_point_.value();
}

void QuickStartController::PrepareForUpdate(bool is_forced) {
  QuickStartMetrics::RecordUpdateStarted(is_forced);
  bootstrap_controller_->PrepareForUpdate();
}

void QuickStartController::ResumeSessionAfterCancelledUpdate() {
  QuickStartMetrics::RecordConsumerUpdateCancelled();
  LoginDisplayHost::default_host()
      ->GetWizardContext()
      ->quick_start_setup_ongoing = true;
  controller_state_ = ControllerState::WAITING_TO_RESUME_AFTER_UPDATE;
}

void QuickStartController::RecordFlowFinished() {
  // State has already been reset when SETUP_COMPLETE UI is shown.
  // We still want to record how long user viewed this final UI.
  metrics_->RecordScreenClosed(
      QuickStartMetrics::ScreenName::kQSComplete,
      QuickStartMetrics::ScreenClosedReason::kSetupComplete);
}

void QuickStartController::InitTargetDeviceBootstrapController() {
  CHECK(LoginDisplayHost::default_host()) << "Missing LoginDisplayHost";
  CHECK(!bootstrap_controller_) << "Expected to not have bootstrap_controller_";

  if (should_resume_quick_start_after_update_) {
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->quick_start_setup_ongoing = true;
    controller_state_ = ControllerState::WAITING_TO_RESUME_AFTER_UPDATE;
  }

  StartObservingScreenTransitions();
  LoginDisplayHost::default_host()->GetWizardContext()->quick_start_enabled =
      true;
  bootstrap_controller_ =
      LoginDisplayHost::default_host()->GetQuickStartBootstrapController();

  // Start observing and determine the discoverable name.
  bootstrap_controller_->AddObserver(this);
}

void QuickStartController::OnGetQuickStartFeatureSupportStatus(
    EntryPointButtonVisibilityCallback set_button_visibility_callback,
    TargetDeviceConnectionBroker::FeatureSupportStatus status) {
  // Maybe prevent a delayed repeated call from TargetDeviceConnectionBroker by
  // re-checking that the flow is not ongoing.
  const bool visible =
      !IsSetupOngoing() &&
      status == TargetDeviceConnectionBroker::FeatureSupportStatus::kSupported;

  // Make the entry point button visible when supported, otherwise keep hidden.
  std::move(set_button_visibility_callback).Run(visible);
}

void QuickStartController::OnStatusChanged(
    const TargetDeviceBootstrapController::Status& status) {
  using Step = TargetDeviceBootstrapController::Step;
  using ErrorCode = TargetDeviceBootstrapController::ErrorCode;

  switch (status.step) {
    case Step::ADVERTISING_WITH_QR_CODE:
      controller_state_ = ControllerState::ADVERTISING;
      CHECK(absl::holds_alternative<QRCode>(status.payload))
          << "Missing expected QR Code data";
      qr_code_ = absl::get<QRCode>(status.payload);
      UpdateUiState(UiState::SHOWING_QR);
      return;
    case Step::ADVERTISING_WITHOUT_QR_CODE:
      UpdateUiState(UiState::CONNECTING_TO_PHONE);
      return;
    case Step::PIN_VERIFICATION:
      CHECK(absl::holds_alternative<PinString>(status.payload))
          << "Missing expected PIN string";
      pin_ = *absl::get<PinString>(status.payload);
      CHECK_EQ(pin_.value().length(), 4UL);
      UpdateUiState(UiState::SHOWING_PIN);
      return;
    case Step::CONNECTED:
      controller_state_ = ControllerState::CONNECTED;
      OnPhoneConnectionEstablished();
      return;
    case Step::REQUESTING_WIFI_CREDENTIALS:
      CHECK(did_request_wifi_credentials_) << "Unrequested WiFi credentials!";
      UpdateUiState(UiState::CONNECTING_TO_WIFI);
      return;
    case Step::WIFI_CREDENTIALS_RECEIVED:
      CHECK(did_request_wifi_credentials_) << "Unrequested WiFi credentials!";
      CHECK(absl::holds_alternative<mojom::WifiCredentials>(status.payload))
          << "Missing expected WifiCredentials";

      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->quick_start_wifi_credentials =
          absl::get<mojom::WifiCredentials>(status.payload);
      ABSL_FALLTHROUGH_INTENDED;
    case Step::EMPTY_WIFI_CREDENTIALS_RECEIVED:
      CHECK(did_request_wifi_credentials_) << "Unrequested WiFi credentials!";
      UpdateUiState(UiState::WIFI_CREDENTIALS_RECEIVED);
      return;
    case Step::REQUESTING_GOOGLE_ACCOUNT_INFO:
      CHECK(did_request_account_info_) << "Unrequested account info received!";
      return;
    case Step::GOOGLE_ACCOUNT_INFO_RECEIVED:
      CHECK(did_request_account_info_) << "Unrequested account info received!";
      CHECK(absl::holds_alternative<EmailString>(status.payload))
          << "Missing expected EmailString";
      // If there aren't any accounts on the phone, the flow is aborted.
      if (absl::get<EmailString>(status.payload)->empty()) {
        QS_LOG(ERROR) << "No account on Android phone. No email received.";
        QuickStartMetrics::RecordGaiaTransferResult(
            /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
                GaiaTransferResultFailureReason::kNoAccountOnPhone);
        AbortFlow(AbortFlowReason::ERROR);
        return;
      }

      // Populate the 'UserInfo' that is shown on the UI and start the transfer.
      user_info_.email = *absl::get<EmailString>(status.payload);
      UpdateUiState(UiState::SIGNING_IN);
      did_request_account_transfer_ = true;
      bootstrap_controller_->AttemptGoogleAccountTransfer();
      return;
    case Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      CHECK(did_request_account_transfer_) << "Unrequested account transfer!";
      // Intermediate state. Nothing to do.
      if (controller_state_ != ControllerState::CONNECTED) {
        QS_LOG(ERROR) << "Expected controller_state_ to be CONNECTED. Actual "
                         "controller_state_: "
                      << controller_state_;
        AbortFlow(AbortFlowReason::ERROR);
      }
      return;
    case Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      CHECK(did_request_account_transfer_) << "Unrequested account transfer!";
      if (controller_state_ != ControllerState::CONNECTED) {
        QS_LOG(ERROR) << "Expected controller_state_ to be CONNECTED. Actual "
                         "controller_state_: "
                      << controller_state_;
        QuickStartMetrics::RecordGaiaTransferResult(
            /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
                GaiaTransferResultFailureReason::kConnectionLost);
        AbortFlow(AbortFlowReason::ERROR);
        return;
      }

      if (absl::holds_alternative<
              TargetDeviceBootstrapController::GaiaCredentials>(
              status.payload)) {
        const TargetDeviceBootstrapController::GaiaCredentials gaia_creds =
            absl::get<TargetDeviceBootstrapController::GaiaCredentials>(
                status.payload);
        if (!gaia_creds.auth_code.empty()) {
          QS_LOG(INFO) << "Successfully received an OAuth authorization code.";
          OnOAuthTokenReceived(gaia_creds);
        } else {
          QS_LOG(INFO) << "QuickStart flow will continue via fallback URL";
          CHECK(!gaia_creds.fallback_url_path->empty())
              << "Fallback URL path empty";
          fallback_url_ = gaia_creds.fallback_url_path.value();
          QuickStartMetrics::RecordGaiaTransferResult(
              /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
                  GaiaTransferResultFailureReason::kFallbackURLRequired);
          controller_state_ = ControllerState::FALLBACK_URL_FLOW_ON_GAIA_SCREEN;
          UpdateUiState(UiState::FALLBACK_URL_FLOW);
        }
      } else {
        CHECK(absl::holds_alternative<ErrorCode>(status.payload))
            << "Missing expected ErrorCode";
        QS_LOG(ERROR) << "Error receiving FIDO assertion. Error Code = "
                      << static_cast<int>(absl::get<ErrorCode>(status.payload));
        QuickStartMetrics::RecordGaiaTransferResult(
            /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
                GaiaTransferResultFailureReason::kErrorReceivingFIDOAssertion);

        // TODO(b:286873060) - Implement retry mechanism/graceful exit.
        NOTIMPLEMENTED();
      }
      return;
    case Step::NONE:
      // Indicates we've stopped advertising and are not connected to the source
      // device. No action required.
      return;
    case Step::ERROR:
      if (absl::holds_alternative<ErrorCode>(status.payload)) {
        QS_LOG(ERROR) << absl::get<ErrorCode>(status.payload);
      } else {
        QS_LOG(ERROR) << "Missing ErrorCode.";
      }
      AbortFlow(AbortFlowReason::ERROR);
      return;
    case Step::FLOW_ABORTED:
      return;
    case Step::SETUP_COMPLETE:
      ResetState();
      return;
  }
}

void QuickStartController::OnCurrentScreenChanged(OobeScreenId previous_screen,
                                                  OobeScreenId current_screen) {
  current_screen_ = current_screen;
  previous_screen_ = previous_screen;

  QS_LOG(INFO) << "Current screen changed from " << previous_screen << " to "
               << current_screen;

  if (current_screen_ == QuickStartScreenHandler::kScreenId) {
    // Just switched into the quick start screen. The ScreenOpened and
    // ScreenClosed metrics are recorded from UpdateUiState() in this case.
    is_transitioning_to_quick_start_screen_ = true;
    HandleTransitionToQuickStartScreen();
  } else if (IsSetupOngoing()) {
    QuickStartMetrics::ScreenName previous_screen_name =
        previous_screen == QuickStartScreenHandler::kScreenId
            ? ScreenNameFromUiState(ui_state_, controller_state_)
            : ScreenNameFromOobeScreenId(previous_screen);
    metrics_->RecordScreenClosed(
        previous_screen_name,
        QuickStartMetrics::ScreenClosedReason::kAdvancedInFlow);
    metrics_->RecordScreenOpened(ScreenNameFromOobeScreenId(current_screen));

    // Detect when the user leaves the Gaia screen during the fallback flow.
    if (controller_state_ ==
            ControllerState::FALLBACK_URL_FLOW_ON_GAIA_SCREEN &&
        previous_screen_ == GaiaScreenHandler::kScreenId) {
      QS_LOG(INFO) << "Gaia screen dismissed while handling fallback URL flow.";
      if (current_screen_ == ErrorScreenHandler::kScreenId) {
        AbortFlow(AbortFlowReason::ERROR);
      } else {
        AbortFlow(AbortFlowReason::USER_CLICKED_BACK);
      }
    }
  }
}

void QuickStartController::OnDestroyingOobeUI() {
  observation_.Reset();
}

void QuickStartController::OnOAuthTokenReceived(
    TargetDeviceBootstrapController::GaiaCredentials gaia_creds) {
  gaia_creds_ = gaia_creds;

  if (gaia_creds_.gaia_id.empty()) {
    QS_LOG(ERROR) << "Obfuscated Gaia ID missing!";
    QuickStartMetrics::RecordGaiaTransferResult(
        /*succeeded=*/false, /*failure_reason=*/QuickStartMetrics::
            GaiaTransferResultFailureReason::kObfuscatedGaiaIdMissing);
    AbortFlow(AbortFlowReason::ERROR);
    return;
  }

  FinishAccountCreation();
}

void QuickStartController::StartObservingScreenTransitions() {
  CHECK(LoginDisplayHost::default_host()) << "Missing LoginDisplayHost";
  CHECK(LoginDisplayHost::default_host()->GetOobeUI()) << "Missing Oobe UI";
  observation_.Observe(LoginDisplayHost::default_host()->GetOobeUI());
}

void QuickStartController::HandleTransitionToQuickStartScreen() {
  CHECK(current_screen_ == QuickStartScreenHandler::kScreenId)
      << "Unexpected current_screen_";

  // No ongoing setup. Entering the screen via entry point.
  if (!IsSetupOngoing()) {
    // Initially there is no UI step. TargetDeviceBootstrapController
    // then determines whether a loading spinner (for the PIN case),
    // or the QR code will be shown. If bluetooth is not turned on, a dialog
    // is shown asking the user for their permission first.
    CHECK(!ui_state_.has_value()) << "Found UI state without ongoing setup!";

    // Keep track of where the flow originated.
    CHECK(!entry_point_.has_value()) << "Entry point without ongoing setup";
    const auto entry_point = EntryPointFromScreen(previous_screen_.value());
    CHECK(entry_point.has_value()) << "Unknown entry point!";
    exit_point_ = entry_point_ = entry_point;
    QuickStartMetrics::RecordEntryPoint(entry_point.value());

    // Set the QuickStart flow as ongoing for the rest of the system.
    LoginDisplayHost::default_host()
        ->GetWizardContext()
        ->quick_start_setup_ongoing = true;

    if (IsBluetoothDisabled()) {
      controller_state_ = ControllerState::WAITING_FOR_BLUETOOTH_PERMISSION;
      UpdateUiState(UiState::SHOWING_BLUETOOTH_DIALOG);
      return;
    }

    StartAdvertising();
  } else if (controller_state_ ==
             ControllerState::WAITING_TO_RESUME_AFTER_UPDATE) {
    exit_point_ = QuickStartController::EntryPoint::GAIA_INFO_SCREEN;

    // It's possible the local state still needs to be cleared if an update was
    // initiated but cancelled. We can't check/clear the state immediately upon
    // cancelling the update since it's possible it happens before the target
    // device persists this pref to local state.
    if (g_browser_process->local_state()->GetBoolean(
            prefs::kShouldResumeQuickStartAfterReboot)) {
      g_browser_process->local_state()->ClearPref(
          prefs::kShouldResumeQuickStartAfterReboot);
    }

    if (IsBluetoothDisabled()) {
      controller_state_ = ControllerState::WAITING_FOR_BLUETOOTH_PERMISSION;
      UpdateUiState(UiState::SHOWING_BLUETOOTH_DIALOG);
      return;
    }

    StartAdvertising();
  } else {
    // If the setup has finished, transitioning to QuickStart should
    // show the last step of the flow.
    if (controller_state_ == ControllerState::SETUP_COMPLETE) {
      UpdateUiState(UiState::SETUP_COMPLETE);
      SavePhoneInstanceID();
      bootstrap_controller_->OnSetupComplete();
      QuickStartMetrics::RecordSetupComplete();
      return;
    }

    // The flow must be resuming after reaching the GaiaInfoScreen or
    // GaiaScreen. Note the the GaiaInfoScreen/GaiaScreen is technically never
    // shown when it switches to QuickStart, so |previous_screen_| is one of the
    // many screens that may have appeared up to this point.
    // TODO(b:283965994) - Improve the resume logic.

    // OOBE flow cannot go back after enrollment checks, update exit point.
    exit_point_ = QuickStartController::EntryPoint::GAIA_INFO_SCREEN;

    if (controller_state_ != ControllerState::CONNECTED) {
      QS_LOG(ERROR) << "Expected controller_state_ to be CONNECTED. Actual "
                       "controller_state_: "
                    << controller_state_;
      AbortFlow(AbortFlowReason::ERROR);
      return;
    }

    CHECK(LoginDisplayHost::default_host()
              ->GetWizardContext()
              ->quick_start_setup_ongoing)
        << "Expected quick_start_setup_ongoing";
    StartAccountTransfer();
  }
}

void QuickStartController::StartAccountTransfer() {
  UpdateUiState(UiState::CONFIRM_GOOGLE_ACCOUNT);
  QuickStartMetrics::RecordGaiaTransferStarted();
  did_request_account_info_ = true;
  bootstrap_controller_->RequestGoogleAccountInfo();
}

void QuickStartController::OnPhoneConnectionEstablished() {
  bootstrap_controller_->StopAdvertising();

  // If cancelling the flow would end on the welcome or network screen,
  // we are still early in the OOBE flow. Transfer WiFi creds if not already
  // connected.
  if (exit_point_ == EntryPoint::WELCOME_SCREEN ||
      exit_point_ == EntryPoint::NETWORK_SCREEN) {
    if (IsConnectedToWiFi()) {
      // This will cause the QuickStartScreen to exit and the NetworkScreen
      // will be shown next.
      UpdateUiState(UiState::WIFI_CREDENTIALS_RECEIVED);
    } else {
      did_request_wifi_credentials_ = true;
      bootstrap_controller_->AttemptWifiCredentialTransfer();
    }
  } else {
    // We are after the 'Gaia Info' screen. Transfer credentials.
    StartAccountTransfer();
  }
}

void QuickStartController::SavePhoneInstanceID() {
  CHECK(bootstrap_controller_) << "Missing bootstrap_controller_";
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

void QuickStartController::FinishAccountCreation() {
  CHECK(!gaia_creds_.email.empty()) << "Missing Gaia email";
  CHECK(!gaia_creds_.gaia_id.empty()) << "Missing Gaia ID";
  CHECK(!gaia_creds_.auth_code.empty()) << "Missing Gaia auth code";

  UpdateUiState(UiState::CREATING_ACCOUNT);
  controller_state_ = ControllerState::SETUP_COMPLETE;
  QuickStartMetrics::RecordGaiaTransferResult(
      /*succeeded=*/true, /*failure_reason=*/std::nullopt);

  const AccountId account_id = AccountId::FromNonCanonicalEmail(
      gaia_creds_.email, gaia_creds_.gaia_id, AccountType::GOOGLE);
  // The user type is known to be regular. The unicorn flow transitions to the
  // Gaia screen and uses its own mechanism for account creation.
  std::unique_ptr<UserContext> user_context =
      login::BuildUserContextForGaiaSignIn(
          /*user_type=*/user_manager::UserType::kRegular,
          /*account_id=*/account_id,
          /*using_saml=*/false,
          /*using_saml_api=*/false,
          /*password=*/"",
          /*password_attributes=*/SamlPasswordAttributes(),
          /*sync_trusted_vault_keys=*/std::nullopt,
          /*challenge_response_key=*/std::nullopt);
  user_context->SetAuthCode(gaia_creds_.auth_code);

  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->CompleteLogin(*user_context);
  }
}

void QuickStartController::ResetState() {
  entry_point_.reset();
  fallback_url_.reset();
  qr_code_.reset();
  pin_.reset();
  user_info_ = UserInfo();
  gaia_creds_ = TargetDeviceBootstrapController::GaiaCredentials();
  wifi_name_.reset();
  controller_state_ = ControllerState::NOT_ACTIVE;
  ui_state_.reset();
  auto* wizard_context = LoginDisplayHost::default_host()->GetWizardContext();
  wizard_context->quick_start_setup_ongoing = false;
  wizard_context->quick_start_wifi_credentials.reset();
  did_request_wifi_credentials_ = false;
  did_request_account_info_ = false;
  did_request_account_transfer_ = false;
  // Don't cleanup |bootstrap_controller_| state here, since it may be waiting
  // for source device to gracefully drop connection.
}

/******************* Bluetooth dialog related functions *******************/

void QuickStartController::StartObservingBluetoothState() {
  GetBluetoothConfigService(
      cros_bluetooth_config_remote_.BindNewPipeAndPassReceiver());
  cros_bluetooth_config_remote_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

void QuickStartController::OnPropertiesUpdated(
    bluetooth_config::mojom::BluetoothSystemPropertiesPtr properties) {
  if (bluetooth_system_state_ == properties->system_state) {
    return;
  }

  bluetooth_system_state_ = properties->system_state;

  if (!IsSetupOngoing()) {
    return;
  }

  QS_LOG(INFO) << "New Bluetooth state: "
               << GetBluetoothStateString(bluetooth_system_state_);
  if (controller_state_ == ControllerState::WAITING_FOR_BLUETOOTH_PERMISSION ||
      controller_state_ == ControllerState::WAITING_FOR_BLUETOOTH_ACTIVATION) {
    if (bluetooth_system_state_ == BluetoothSystemState::kEnabled) {
      StartAdvertising();
    }
  }
}

bool QuickStartController::IsBluetoothDisabled() {
  return bluetooth_system_state_ == BluetoothSystemState::kDisabled;
}

bool QuickStartController::WillRequestWiFi() {
  return !IsConnectedToWiFi();
}

void QuickStartController::OnFallbackUrlFlowSuccess() {
  if (controller_state_ == ControllerState::FALLBACK_URL_FLOW_ON_GAIA_SCREEN) {
    SavePhoneInstanceID();
    controller_state_ = ControllerState::SETUP_COMPLETE;
  }
}

void QuickStartController::OnBluetoothPermissionGranted() {
  if (controller_state_ != ControllerState::WAITING_FOR_BLUETOOTH_PERMISSION) {
    return;
  }

  controller_state_ = ControllerState::WAITING_FOR_BLUETOOTH_ACTIVATION;

  if (IsBluetoothDisabled()) {
    CHECK(cros_bluetooth_config_remote_)
        << "Missing cros_bluetooth_config_remote_";
    cros_bluetooth_config_remote_->SetBluetoothEnabledWithoutPersistence();
    // Advertising will start once we are notified that bluetooth is enabled.
  }
}

void QuickStartController::StartAdvertising() {
  QS_LOG(INFO) << "ControllerState::INITIALIZING requesting advertising.";
  controller_state_ = ControllerState::INITIALIZING;
  bootstrap_controller_->StartAdvertisingAndMaybeGetQRCode();
}

std::ostream& operator<<(std::ostream& stream,
                         const QuickStartController::UiState& ui_state) {
  switch (ui_state) {
    case QuickStartController::UiDelegate::UiState::SHOWING_BLUETOOTH_DIALOG:
      stream << "[showing Bluetooth dialog]";
      break;
    case QuickStartController::UiDelegate::UiState::CONNECTING_TO_PHONE:
      stream << "[connecting to phone]";
      break;
    case QuickStartController::UiDelegate::UiState::SHOWING_QR:
      stream << "[showing QR]";
      break;
    case QuickStartController::UiDelegate::UiState::SHOWING_PIN:
      stream << "[showing PIN]";
      break;
    case QuickStartController::UiDelegate::UiState::CONNECTING_TO_WIFI:
      stream << "[connecting to WiFi]";
      break;
    case QuickStartController::UiDelegate::UiState::WIFI_CREDENTIALS_RECEIVED:
      stream << "[WiFi credentials received]";
      break;
    case QuickStartController::UiDelegate::UiState::CONFIRM_GOOGLE_ACCOUNT:
      stream << "[confirm Google account]";
      break;
    case QuickStartController::UiDelegate::UiState::SIGNING_IN:
      stream << "[signing in]";
      break;
    case QuickStartController::UiDelegate::UiState::CREATING_ACCOUNT:
      stream << "[creating account]";
      break;
    case QuickStartController::UiDelegate::UiState::FALLBACK_URL_FLOW:
      stream << "[fallback URL flow]";
      break;
    case QuickStartController::UiDelegate::UiState::SETUP_COMPLETE:
      stream << "[setup complete]";
      break;
    case QuickStartController::UiDelegate::UiState::EXIT_SCREEN:
      stream << "[exit screen]";
      break;
  }

  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartController::AbortFlowReason& abort_flow_reason) {
  switch (abort_flow_reason) {
    case QuickStartController::AbortFlowReason::USER_CLICKED_BACK:
      stream << "[user clicked back]";
      break;
    case QuickStartController::AbortFlowReason::USER_CLICKED_CANCEL:
      stream << "[user clicked cancel]";
      break;
    case QuickStartController::AbortFlowReason::SIGNIN_SCHOOL:
      stream << "[signin school]";
      break;
    case QuickStartController::AbortFlowReason::ENTERPRISE_ENROLLMENT:
      stream << "[enterprise enrollment]";
      break;
    case QuickStartController::AbortFlowReason::ERROR:
      stream << "[error]";
      break;
    case QuickStartController::AbortFlowReason::ADD_CHILD:
      stream << "[add child]";
      break;
  }

  return stream;
}

std::ostream& operator<<(
    std::ostream& stream,
    const QuickStartController::ControllerState& controller_state) {
  switch (controller_state) {
    case QuickStartController::ControllerState::NOT_ACTIVE:
      stream << "[not active]";
      break;
    case QuickStartController::ControllerState::
        WAITING_FOR_BLUETOOTH_PERMISSION:
      stream << "[waiting for bluetooth permission]";
      break;
    case QuickStartController::ControllerState::
        WAITING_FOR_BLUETOOTH_ACTIVATION:
      stream << "[waiting for bluetooth activation]";
      break;
    case QuickStartController::ControllerState::WAITING_TO_RESUME_AFTER_UPDATE:
      stream << "[waiting to resume after update]";
      break;
    case QuickStartController::ControllerState::INITIALIZING:
      stream << "[initializing]";
      break;
    case QuickStartController::ControllerState::ADVERTISING:
      stream << "[advertising]";
      break;
    case QuickStartController::ControllerState::CONNECTED:
      stream << "[connected]";
      break;
    case QuickStartController::ControllerState::
        CONTINUING_AFTER_ENROLLMENT_CHECKS:
      stream << "[continuing after enrollment checks]";
      break;
    case QuickStartController::ControllerState::
        FALLBACK_URL_FLOW_ON_GAIA_SCREEN:
      stream << "[fallback URL flow on Gaia screen]";
      break;
    case QuickStartController::ControllerState::SETUP_COMPLETE:
      stream << "[setup complete]";
      break;
  }

  return stream;
}

}  // namespace ash::quick_start
