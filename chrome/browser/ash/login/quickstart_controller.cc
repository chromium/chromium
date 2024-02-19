// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/quickstart_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "base/check.h"
#include "base/logging.h"
#include "chrome/browser/ash/login/oobe_quick_start/connectivity/target_device_connection_broker.h"
#include "chrome/browser/ash/login/oobe_quick_start/oobe_quick_start_pref_names.h"
#include "chrome/browser/ash/login/oobe_quick_start/target_device_bootstrap_controller.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/ui/login_display_host.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/ui/webui/ash/login/consumer_update_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_info_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/gaia_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/browser/ui/webui/ash/login/online_login_utils.h"
#include "chrome/browser/ui/webui/ash/login/quick_start_screen_handler.h"
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
#include "google_apis/gaia/gaia_constants.h"
#include "google_apis/gaia/gaia_oauth_client.h"

namespace ash::quick_start {

namespace {

constexpr int kMaxRetryAttempts = 3;

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
    OobeScreenId screen_id) {
  //  TODO(b/298042953): Check Screen IDs for Unicorn account setup flow.
  if (screen_id == ConsumerUpdateScreenView::kScreenId) {
    //  TODO(b/298042953): Update Screen ID when the new OOBE Checking for
    //  update and determining device configuration screen is added.
    return QuickStartMetrics::ScreenName::
        kCheckingForUpdateAndDeterminingDeviceConfiguration;
  } else if (screen_id == UserCreationView::kScreenId) {
    return QuickStartMetrics::ScreenName::kChooseChromebookSetup;
  }
  return QuickStartMetrics::ScreenName::kOther;
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
    case QuickStartController::AbortFlowReason::ENTERPRISE_ENROLLMENT:
      return TargetDeviceBootstrapController::ConnectionClosedReason::
          kUserAborted;
    case QuickStartController::AbortFlowReason::QUICK_START_FLOW_COMPLETE:
      return TargetDeviceBootstrapController::ConnectionClosedReason::kComplete;
    case QuickStartController::AbortFlowReason::ERROR:
      return TargetDeviceBootstrapController::ConnectionClosedReason::
          kUnknownError;
  }
}

gaia::OAuthClientInfo GetClientInfo() {
  gaia::OAuthClientInfo client_info;
  client_info.client_id = GaiaUrls::GetInstance()->oauth2_chrome_client_id();
  client_info.client_secret =
      GaiaUrls::GetInstance()->oauth2_chrome_client_secret();
  return client_info;
}

}  // namespace

QuickStartController::QuickStartController() {
  gaia_client_ = std::make_unique<gaia::GaiaOAuthClient>(
      g_browser_process->shared_url_loader_factory());
  // Main feature flag
  if (!features::IsOobeQuickStartEnabled()) {
    return;
  }

  // QuickStart may not be available on the login screen.
  if (session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::OOBE &&
      !features::IsOobeQuickStartOnLoginScreenEnabled()) {
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

void QuickStartController::UpdateUiState(UiState ui_state) {
  QS_LOG(INFO) << "Updating UI state to " << ui_state;
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
  QS_LOG(INFO) << "Force enabling LocalPasswordsForConsumers!";
  ash::features::ForceEnableLocalPasswordsForConsumers();
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

void QuickStartController::AbortFlow(AbortFlowReason reason) {
  CHECK(bootstrap_controller_);
  QS_LOG(INFO) << "Aborting flow.";

  bootstrap_controller_->CloseOpenConnections(
      ConnectionClosedReasonFromAbortFlowReason(reason));
  bootstrap_controller_->StopAdvertising();
  ResetState();

  // Triggers a screen exit if there is a UiDelegate driving the UI.
  if (!ui_delegates_.empty()) {
    CHECK(current_screen_ == QuickStartScreenHandler::kScreenId ||
          current_screen_ == NetworkScreenHandler::kScreenId);
    ui_delegates_.begin()->OnUiUpdateRequested(UiState::EXIT_SCREEN);
  }
}

QuickStartController::EntryPoint QuickStartController::GetExitPoint() {
  return exit_point_.value();
}

void QuickStartController::PrepareForUpdate() {
  // TODO(b/280308569): Investigate whether state should be reset here in case
  // of error installing update.
  bootstrap_controller_->PrepareForUpdate();
}

void QuickStartController::InitTargetDeviceBootstrapController() {
  CHECK(LoginDisplayHost::default_host());
  CHECK(!bootstrap_controller_);

  if (g_browser_process->local_state()->GetBoolean(
          prefs::kShouldResumeQuickStartAfterReboot)) {
    g_browser_process->local_state()->ClearPref(
        prefs::kShouldResumeQuickStartAfterReboot);
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

  // TODO(b/298042953): Emit ScreenOpened metrics when automatically
  // resuming after an update.
  switch (status.step) {
    case Step::ADVERTISING_WITH_QR_CODE:
      controller_state_ = ControllerState::ADVERTISING;
      CHECK(absl::holds_alternative<QRCode::PixelData>(status.payload));
      qr_code_data_ = absl::get<QRCode::PixelData>(status.payload);
      UpdateUiState(UiState::SHOWING_QR);
      QuickStartMetrics::RecordScreenOpened(
          QuickStartMetrics::ScreenName::kSetUpAndroidPhone);
      return;
    case Step::ADVERTISING_WITHOUT_QR_CODE:
      UpdateUiState(UiState::CONNECTING_TO_PHONE);
      return;
    case Step::PIN_VERIFICATION:
      CHECK(absl::holds_alternative<PinString>(status.payload));
      pin_ = *absl::get<PinString>(status.payload);
      CHECK(pin_.value().length() == 4);
      UpdateUiState(UiState::SHOWING_PIN);
      QuickStartMetrics::RecordScreenOpened(
          QuickStartMetrics::ScreenName::kSetUpAndroidPhone);
      return;
    case Step::CONNECTED:
      controller_state_ = ControllerState::CONNECTED;
      OnPhoneConnectionEstablished();
      return;
    case Step::REQUESTING_WIFI_CREDENTIALS:
      UpdateUiState(UiState::CONNECTING_TO_WIFI);
      QuickStartMetrics::RecordScreenOpened(
          QuickStartMetrics::ScreenName::kConnectingToWifi);
      return;
    case Step::WIFI_CREDENTIALS_RECEIVED:
      CHECK(absl::holds_alternative<mojom::WifiCredentials>(status.payload));

      LoginDisplayHost::default_host()
          ->GetWizardContext()
          ->quick_start_wifi_credentials =
          absl::get<mojom::WifiCredentials>(status.payload);
      ABSL_FALLTHROUGH_INTENDED;
    case Step::EMPTY_WIFI_CREDENTIALS_RECEIVED:
      UpdateUiState(UiState::WIFI_CREDENTIALS_RECEIVED);
      return;
    case Step::REQUESTING_GOOGLE_ACCOUNT_INFO:
      return;
    case Step::GOOGLE_ACCOUNT_INFO_RECEIVED:
      CHECK(absl::holds_alternative<EmailString>(status.payload));
      // If there aren't any accounts on the phone, the flow is aborted.
      if (absl::get<EmailString>(status.payload)->empty()) {
        QS_LOG(ERROR) << "No account on Android phone. No email received.";
        AbortFlow(AbortFlowReason::ERROR);
        return;
      }

      // Populate the 'UserInfo' that is shown on the UI and start the transfer.
      user_info_.email = *absl::get<EmailString>(status.payload);
      UpdateUiState(UiState::SIGNING_IN);
      bootstrap_controller_->AttemptGoogleAccountTransfer();
      return;
    case Step::TRANSFERRING_GOOGLE_ACCOUNT_DETAILS:
      // Intermediate state. Nothing to do.
      CHECK(controller_state_ == ControllerState::CONNECTED);
      // TODO(b/298042953): Record Gaia Transfer screen shown once UI is
      // implemented.
      return;
    case Step::TRANSFERRED_GOOGLE_ACCOUNT_DETAILS:
      CHECK(controller_state_ == ControllerState::CONNECTED);
      if (absl::holds_alternative<
              TargetDeviceBootstrapController::GaiaCredentials>(
              status.payload)) {
        QS_LOG(INFO) << "Successfully received an OAuth authorization code.";
        OnOAuthTokenReceived(
            absl::get<TargetDeviceBootstrapController::GaiaCredentials>(
                status.payload));
      } else {
        CHECK(absl::holds_alternative<ErrorCode>(status.payload));
        QS_LOG(ERROR) << "Error receiving FIDO assertion. Error Code = "
                      << static_cast<int>(absl::get<ErrorCode>(status.payload));

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
      [[fallthrough]];
    case Step::SETUP_COMPLETE:
      return;
  }
}

void QuickStartController::OnCurrentScreenChanged(OobeScreenId previous_screen,
                                                  OobeScreenId current_screen) {
  current_screen_ = current_screen;
  previous_screen_ = previous_screen;

  if (current_screen_ == QuickStartScreenHandler::kScreenId) {
    // Just switched into the quick start screen. The ScreenOpened metrics on
    // the Quick Start screen are recorded from OnStatusChanged().
    HandleTransitionToQuickStartScreen();
  } else if (IsSetupOngoing()) {
    QuickStartMetrics::RecordScreenOpened(
        ScreenNameFromOobeScreenId(current_screen));
  }
}

void QuickStartController::OnDestroyingOobeUI() {
  observation_.Reset();
}

void QuickStartController::OnOAuthTokenReceived(
    TargetDeviceBootstrapController::GaiaCredentials gaia_creds) {
  gaia_creds_ = gaia_creds;

  // TODO(b/319631013) - Track BootstrapConfiguration email mismatch via UMA.
  QS_LOG(INFO) << "About to exchange authorization code for tokens.";
  gaia_client_->GetTokensFromAuthCode(GetClientInfo(), gaia_creds_.auth_code,
                                      kMaxRetryAttempts, this);
}

void QuickStartController::OnGetTokensResponse(const std::string& refresh_token,
                                               const std::string& access_token,
                                               int expires_in_seconds) {
  QS_LOG(INFO) << "Successfully exchanged the authorization code for tokens.";

  gaia_creds_.auth_code = "";
  gaia_creds_.access_token = access_token;
  gaia_creds_.refresh_token = refresh_token;

  // Get an access token with userinfo scope for fetching the GaiaID.
  QS_LOG(INFO) << "Requesting access token with userinfo scope.";
  gaia_client_->RefreshToken(GetClientInfo(), gaia_creds_.refresh_token,
                             {GaiaConstants::kGoogleUserInfoProfile},
                             kMaxRetryAttempts, this);
}

void QuickStartController::OnRefreshTokenResponse(
    const std::string& access_token,
    int expires_in_seconds) {
  QS_LOG(INFO) << "Received the access token. Requesting user information.";
  gaia_client_->GetUserInfo(access_token, kMaxRetryAttempts, this);
}

void QuickStartController::OnGetUserInfoResponse(
    const base::Value::Dict& user_info) {
  QS_LOG(INFO) << "Successfully retrieved user information.";

  const std::string* gaia_id_value = user_info.FindString("id");
  if (!gaia_id_value || gaia_id_value->empty()) {
    QS_LOG(ERROR) << "Obfuscated Gaia ID not found!";
    AbortFlow(AbortFlowReason::ERROR);
    return;
  }
  gaia_creds_.gaia_id = *gaia_id_value;

  FinishAccountCreation();
}

void QuickStartController::OnOAuthError() {
  QS_LOG(ERROR) << "An authorization error occurred!";
  AbortFlow(AbortFlowReason::ERROR);
}

void QuickStartController::OnNetworkError(int response_code) {
  QS_LOG(ERROR) << "A network error occurred " << response_code;
  AbortFlow(AbortFlowReason::ERROR);
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
      return;
    }

    // The flow must be resuming after reaching the GaiaInfoScreen or
    // GaiaScreen. Note the the GaiaInfoScreen/GaiaScreen is technically never
    // shown when it switches to QuickStart, so |previous_screen_| is one of the
    // many screens that may have appeared up to this point.
    // TODO(b:283965994) - Imrpve the resume logic.
    CHECK(controller_state_ == ControllerState::CONNECTED);
    CHECK(LoginDisplayHost::default_host()
              ->GetWizardContext()
              ->quick_start_setup_ongoing);

    // OOBE flow cannot go back after enrollment checks, update exit point.
    exit_point_ = QuickStartController::EntryPoint::GAIA_INFO_SCREEN;

    StartAccountTransfer();
  }
}

void QuickStartController::StartAccountTransfer() {
  UpdateUiState(UiState::CONFIRM_GOOGLE_ACCOUNT);
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
      bootstrap_controller_->AttemptWifiCredentialTransfer();
    }
  } else {
    // We are after the 'Gaia Info' screen. Transfer credentials.
    StartAccountTransfer();
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

void QuickStartController::FinishAccountCreation() {
  CHECK(!gaia_creds_.email.empty());
  CHECK(!gaia_creds_.gaia_id.empty());

  UpdateUiState(UiState::CREATING_ACCOUNT);
  controller_state_ = ControllerState::SETUP_COMPLETE;

  const AccountId account_id = AccountId::FromNonCanonicalEmail(
      gaia_creds_.email, gaia_creds_.gaia_id, AccountType::GOOGLE);
  auto user_context = std::make_unique<UserContext>();
  // The user type is known to be regular. The unicorn flow transitions to the
  // Gaia screen and uses its own mechanism for account creation.
  login::BuildUserContextForGaiaSignIn(
      /*user_type=*/user_manager::UserType::kRegular,
      /*account_id=*/account_id,
      /*using_saml=*/false,
      /*using_saml_api=*/false,
      /*password=*/"",
      /*password_attributes=*/SamlPasswordAttributes(),
      /*sync_trusted_vault_keys=*/std::nullopt,
      /*challenge_response_key=*/std::nullopt,
      /*user_context=*/user_context.get());

  // TODO(b/318664950) - Remove once the server starts sending the Gaia ID.
  user_context->SetRefreshToken(gaia_creds_.refresh_token);
  user_context->SetAccessToken(gaia_creds_.access_token);
  // Since the tokens are being set, the auth code must be empty.
  CHECK(user_context->GetAuthCode().empty());

  if (LoginDisplayHost::default_host()) {
    LoginDisplayHost::default_host()->CompleteLogin(*user_context);
  }
}

void QuickStartController::ResetState() {
  entry_point_.reset();
  qr_code_data_.reset();
  pin_.reset();
  user_info_ = UserInfo();
  gaia_creds_ = TargetDeviceBootstrapController::GaiaCredentials();
  wifi_name_.reset();
  controller_state_ = ControllerState::NOT_ACTIVE;
  ui_state_.reset();
  auto* wizard_context = LoginDisplayHost::default_host()->GetWizardContext();
  wizard_context->quick_start_setup_ongoing = false;
  wizard_context->quick_start_wifi_credentials.reset();
  bootstrap_controller_->Cleanup();
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

void QuickStartController::OnBluetoothPermissionGranted() {
  if (controller_state_ != ControllerState::WAITING_FOR_BLUETOOTH_PERMISSION) {
    return;
  }

  controller_state_ = ControllerState::WAITING_FOR_BLUETOOTH_ACTIVATION;

  if (IsBluetoothDisabled()) {
    CHECK(cros_bluetooth_config_remote_);
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
    case QuickStartController::UiDelegate::UiState::SETUP_COMPLETE:
      stream << "[setup complete]";
      break;
    case QuickStartController::UiDelegate::UiState::EXIT_SCREEN:
      stream << "[exit screen]";
      break;
  }

  return stream;
}

}  // namespace ash::quick_start
