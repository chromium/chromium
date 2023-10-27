// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/screens/network_screen.h"

#include <string>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_controller.h"
#include "chrome/browser/ash/login/helper.h"
#include "chrome/browser/ash/login/oobe_screen.h"
#include "chrome/browser/ash/login/quickstart_controller.h"
#include "chrome/browser/ash/login/startup_utils.h"
#include "chrome/browser/ash/login/wizard_context.h"
#include "chrome/browser/ash/login/wizard_controller.h"
#include "chrome/browser/ui/webui/ash/login/network_screen_handler.h"
#include "chrome/grit/branded_strings.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/technology_state_controller.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom-shared.h"
#include "chromeos/ash/services/nearby/public/mojom/quick_start_decoder_types.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom-shared.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {
namespace {

constexpr base::TimeDelta kConnectionTimeout = base::Seconds(40);

constexpr char kUserActionBackButtonClicked[] = "back";
constexpr char kUserActionCancelButtonClicked[] = "cancel";
constexpr char kUserActionContinueButtonClicked[] = "continue";
constexpr char kUserActionQuickStartButtonClicked[] = "activateQuickStart";

chromeos::network_config::mojom::SecurityType ConvertSecurityType(
    quick_start::mojom::WifiSecurityType type) {
  switch (type) {
    case quick_start::mojom::WifiSecurityType::kPSK:
      return chromeos::network_config::mojom::SecurityType::kWpaPsk;
    case quick_start::mojom::WifiSecurityType::kWEP:
      return chromeos::network_config::mojom::SecurityType::kWepPsk;
    case quick_start::mojom::WifiSecurityType::kEAP:
      return chromeos::network_config::mojom::SecurityType::kWpaEap;
    case quick_start::mojom::WifiSecurityType::kOpen:
    case quick_start::mojom::WifiSecurityType::kOWE:
    case quick_start::mojom::WifiSecurityType::kSAE:
      return chromeos::network_config::mojom::SecurityType::kNone;
  }
}

chromeos::network_config::mojom::ConfigPropertiesPtr CreateNetworkConfig(
    const quick_start::mojom::WifiCredentials& wifi_credentials) {
  auto wifi = chromeos::network_config::mojom::WiFiConfigProperties::New();
  wifi->ssid = wifi_credentials.ssid;
  wifi->security = ConvertSecurityType(wifi_credentials.security_type);
  wifi->passphrase = wifi_credentials.password;

  auto config = chromeos::network_config::mojom::ConfigProperties::New();
  config->type_config =
      chromeos::network_config::mojom::NetworkTypeConfigProperties::NewWifi(
          std::move(wifi));

  // Since the details of the connection are coming from a connected phone,
  // static IP addressing is not supported, only DHCP.
  config->name_servers_config_type = onc::network_config::kIPConfigTypeDHCP;

  // Proxy settings are not supported for now.
  return config;
}

}  // namespace

// static
std::string NetworkScreen::GetResultString(Result result) {
  switch (result) {
    case Result::CONNECTED:
      return "Connected";
    case Result::BACK:
      return "Back";
    case Result::QUICK_START:
      return "QuickStart";
    case Result::NOT_APPLICABLE:
      return BaseScreen::kNotApplicable;
  }
}

NetworkScreen::NetworkScreen(base::WeakPtr<NetworkScreenView> view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(NetworkScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {}

NetworkScreen::~NetworkScreen() {
  connection_timer_.Stop();
  UnsubscribeNetworkNotification();
}

bool NetworkScreen::MaybeSkip(WizardContext& context) {
  // Skip this screen if the device is connected to Ethernet for the first time
  // in this session.
  return UpdateStatusIfConnectedToEthernet();
}

void NetworkScreen::ShowImpl() {
  if (!view_) {
    return;
  }

  // In OOBE all physical network technologies should be enabled, so the user is
  // able to select any of the available networks on the device. Enabled
  // technologies should not be changed if network screen is shown outside of
  // OOBE. Right now NetworkScreen is only used in the beginning of OOBE.
  TechnologyStateController* controller =
      NetworkHandler::Get()->technology_state_controller();
  controller->SetTechnologiesEnabled(NetworkTypePattern::Physical(), true,
                                     network_handler::ErrorCallback());
  Refresh();
  // QuickStart should not be enabled for Demo mode or OS Install flows
  if (features::IsOobeQuickStartEnabled() &&
      !DemoSetupController::IsOobeDemoSetupFlowInProgress() &&
      !switches::IsOsInstallAllowed()) {
    // Determine the QuickStart button visibility
    WizardController::default_controller()
        ->quick_start_controller()
        ->DetermineEntryPointVisibility(
            base::BindOnce(&NetworkScreen::SetQuickStartButtonVisibility,
                           weak_ptr_factory_.GetWeakPtr()));
  }

  if (context()->quick_start_setup_ongoing) {
    ShowStepsWhenQuickStartOngoing();
  } else {
    // Shows the typical network list.
    view_->ShowScreenWithData({});
  }
}

void NetworkScreen::HideImpl() {
  connection_timer_.Stop();

  UnsubscribeNetworkNotification();

  WizardController::default_controller()
      ->quick_start_controller()
      ->DetachFrontend(this);
}

void NetworkScreen::OnUserAction(const base::Value::List& args) {
  const std::string& action_id = args[0].GetString();
  if (action_id == kUserActionQuickStartButtonClicked) {
    OnQuickStartButtonClicked();
  } else if (action_id == kUserActionContinueButtonClicked) {
    OnContinueButtonClicked();
  } else if (action_id == kUserActionBackButtonClicked) {
    if (context()->quick_start_setup_ongoing) {
      // Clicking 'Back' (only visible on the actual network list) while
      // QuickStart is going on will cancel the QuickStart flow.
      ExitQuickStartFlow();
    } else {
      OnBackButtonClicked();
    }
  } else if (action_id == kUserActionCancelButtonClicked) {
    CHECK(context()->quick_start_setup_ongoing);
    ExitQuickStartFlow();
  } else {
    BaseScreen::OnUserAction(args);
  }
}

bool NetworkScreen::HandleAccelerator(LoginAcceleratorAction action) {
  if (action == LoginAcceleratorAction::kStartEnrollment) {
    context()->enrollment_triggered_early = true;
    return true;
  }

  return false;
}

void NetworkScreen::NetworkConnectionStateChanged(const NetworkState* network) {
  // If we are in quick start flow we need to either wait for a connection to
  // the network that was shared by the source device or wait for the online
  // state of any network.
  if (context()->quick_start_setup_ongoing && !network->IsOnline()) {
    if (base::UTF8ToUTF16(network->name()) == network_id_ &&
        !network->GetError().empty()) {
      ExitQuickStartFlow();
      return;
    }
  }
  UpdateStatus();
}

void NetworkScreen::DefaultNetworkChanged(const NetworkState* network) {
  UpdateStatus();
}

void NetworkScreen::OnUiUpdateRequested(
    quick_start::QuickStartController::UiState state) {
  if (state == ash::quick_start::QuickStartController::UiState::EXIT_SCREEN) {
    // Controller requested the flow to be aborted.
    WizardController::default_controller()
        ->quick_start_controller()
        ->DetachFrontend(this);
    // Show the standard 'Network List'
    if (view_) {
      view_->ShowScreenWithData({});
    }
  }
}

void NetworkScreen::Refresh() {
  continue_pressed_ = false;
  SubscribeNetworkNotification();
  UpdateStatus();
}

void NetworkScreen::SetNetworkStateHelperForTest(
    login::NetworkStateHelper* helper) {
  network_state_helper_.reset(helper);
}

void NetworkScreen::SubscribeNetworkNotification() {
  if (!is_network_subscribed_) {
    is_network_subscribed_ = true;
    network_state_handler_observer_.Observe(
        NetworkHandler::Get()->network_state_handler());
  }
}

void NetworkScreen::UnsubscribeNetworkNotification() {
  if (is_network_subscribed_) {
    is_network_subscribed_ = false;
    network_state_handler_observer_.Reset();
  }
}

void NetworkScreen::NotifyOnConnection() {
  UnsubscribeNetworkNotification();
  exit_callback_.Run(Result::CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  if (context()->quick_start_setup_ongoing) {
    ExitQuickStartFlow();
    return;
  }
  if (!network_state_helper_->IsConnected() && view_) {
    view_->ShowError(l10n_util::GetStringFUTF16(
        IDS_NETWORK_SELECTION_ERROR,
        l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_OS_NAME), network_id_));
  }
  StopWaitingForConnection(network_id_);
}

void NetworkScreen::UpdateStatus() {
  bool is_connected = network_state_helper_->IsConnected();

  if (view_ && is_connected) {
    view_->ClearErrors();
  }

  std::u16string network_name = network_state_helper_->GetCurrentNetworkName();
  if (is_connected) {
    StopWaitingForConnection(network_name);
  } else if (network_state_helper_->IsConnecting()) {
    WaitForConnection(network_name);
  } else {
    StopWaitingForConnection(network_id_);
  }
}

void NetworkScreen::StopWaitingForConnection(const std::u16string& network_id) {
  const bool is_connected = network_state_helper_->IsConnected();
  // `context()` might not exist in unit tests.
  const bool quick_start_setup_ongoing =
      context() && context()->quick_start_setup_ongoing;
  if (is_connected && (continue_pressed_ || quick_start_setup_ongoing)) {
    NotifyOnConnection();
    return;
  }

  connection_timer_.Stop();

  network_id_ = network_id;

  // Automatically continue if the device is connected to Ethernet for the first
  // time in this session.
  if (UpdateStatusIfConnectedToEthernet()) {
    return;
  }

  // Automatically continue if we are using Zero-Touch Hands-Off Enrollment.
  if (is_connected && continue_attempts_ == 0 &&
      WizardController::IsZeroTouchHandsOffOobeFlow()) {
    OnContinueButtonClicked();
  }
}

void NetworkScreen::WaitForConnection(const std::u16string& network_id) {
  if (network_id_ != network_id || !connection_timer_.IsRunning()) {
    connection_timer_.Stop();
    connection_timer_.Start(FROM_HERE, kConnectionTimeout, this,
                            &NetworkScreen::OnConnectionTimeout);
  }

  network_id_ = network_id;
}

void NetworkScreen::OnBackButtonClicked() {
  if (view_) {
    view_->ClearErrors();
  }

  exit_callback_.Run(Result::BACK);
}

void NetworkScreen::OnContinueButtonClicked() {
  ++continue_attempts_;
  if (view_) {
    view_->ClearErrors();
  }

  if (network_state_helper_->IsConnected()) {
    NotifyOnConnection();
    return;
  }
  continue_pressed_ = true;
  WaitForConnection(network_id_);
}

void NetworkScreen::OnQuickStartButtonClicked() {
  CHECK(context()->quick_start_enabled);
  exit_callback_.Run(Result::QUICK_START);
}

void NetworkScreen::SetQuickStartButtonVisibility(bool visible) {
  if (visible && view_) {
    view_->SetQuickStartEnabled();
  }
}

void NetworkScreen::ConfigureWifiNetwork(
    const quick_start::mojom::WifiCredentials& wifi_credentials) {
  // TODO(b/300389592): Remove error logs.
  LOG(ERROR) << __func__ << " configuring: " << wifi_credentials.ssid;
  network_id_ = base::UTF8ToUTF16(wifi_credentials.ssid);
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());

  auto config = CreateNetworkConfig(wifi_credentials);
  remote_cros_network_config_->ConfigureNetwork(
      std::move(config), /*shared=*/true,
      base::BindOnce(&NetworkScreen::OnConfigureWifiNetworkResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkScreen::OnConfigureWifiNetworkResult(
    const absl::optional<std::string>& network_guid,
    const std::string& error_message) {
  if (!network_guid.has_value() || !error_message.empty()) {
    LOG(ERROR) << "Configure network failed with  "
               << " network_guid: " << network_guid.value_or("none")
               << " and error_message: " << error_message;
    ExitQuickStartFlow();
    return;
  }
  remote_cros_network_config_->StartConnect(
      network_guid.value(),
      base::BindOnce(&NetworkScreen::OnStartConnectCompleted,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkScreen::OnStartConnectCompleted(
    chromeos::network_config::mojom::StartConnectResult result,
    const std::string& message) {
  if (result != chromeos::network_config::mojom::StartConnectResult::kSuccess) {
    LOG(ERROR) << "Start connect failed with result " << result
               << " and message " << message;
    ExitQuickStartFlow();
    return;
  }
  WaitForConnection(network_id_);
}

void NetworkScreen::ExitQuickStartFlow() {
  CHECK(context()->quick_start_setup_ongoing);
  auto* quick_start_controller = LoginDisplayHost::default_host()
                                     ->GetWizardController()
                                     ->quick_start_controller();
  quick_start_controller->DetachFrontend(this);
  const auto entry_point = quick_start_controller->GetExitPoint();
  quick_start_controller->AbortFlow();
  if (entry_point ==
      quick_start::QuickStartController::EntryPoint::NETWORK_SCREEN) {
    // Switches to the screen step that shows the list of networks.
    Show(context());
    return;
  }
  exit_callback_.Run(Result::BACK);
}

void NetworkScreen::ShowStepsWhenQuickStartOngoing() {
  CHECK(context()->quick_start_setup_ongoing);
  // Attach frontend so that the QuickStartController may notify us in case the
  // flow is severed from the phone side, or if an error occurs.
  WizardController::default_controller()
      ->quick_start_controller()
      ->AttachFrontend(this);

  if (context()->quick_start_wifi_credentials.has_value()) {
    // QuickStart WiFi Transfer Screen Step
    const auto credentials = context()->quick_start_wifi_credentials.value();
    context()->quick_start_wifi_credentials.reset();
    ConfigureWifiNetwork(credentials);
    view_->ShowScreenWithData(
        base::Value::Dict().Set("ssid", credentials.ssid));
  } else {
    // QuickStart is ongoing, but no WiFi credentials have been provided.
    // Customize the UI with a specific subtitle informing the user that they
    // need to connect to a network in order to continue setting up with their
    // Android phone.
    view_->ShowScreenWithData(
        base::Value::Dict().Set("useQuickStartSubtitle", true));
  }
}

bool NetworkScreen::UpdateStatusIfConnectedToEthernet() {
  if (switches::IsOOBENetworkScreenSkippingDisabledForTesting()) {
    return false;
  }

  if (!first_ethernet_connection_)
    return false;

  if (!network_state_helper_->IsConnectedToEthernet())
    return false;

  first_ethernet_connection_ = false;

  if (is_hidden()) {
    // Screen not shown yet: skipping it.
    exit_callback_.Run(Result::NOT_APPLICABLE);
  } else {
    // Screen already shown: automatically continuing.
    exit_callback_.Run(Result::CONNECTED);
  }

  return true;
}

}  // namespace ash
