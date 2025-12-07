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
#include "base/task/single_thread_task_runner.h"
#include "base/time/time.h"
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
#include "chromeos/ash/components/quick_start/logging.h"
#include "chromeos/ash/components/quick_start/quick_start_metrics.h"
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
  // LINT.IfChange(UsageMetrics)
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
  // LINT.ThenChange(//tools/metrics/histograms/metadata/oobe/histograms.xml)
}

NetworkScreen::NetworkScreen(base::WeakPtr<NetworkScreenView> view,
                             const ScreenExitCallback& exit_callback)
    : BaseScreen(NetworkScreenView::kScreenId, OobeScreenPriority::DEFAULT),
      view_(std::move(view)),
      exit_callback_(exit_callback),
      network_state_helper_(std::make_unique<login::NetworkStateHelper>()) {
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
}

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

  // Determine the QuickStart button visibility.
  WizardController::default_controller()
      ->quick_start_controller()
      ->DetermineEntryPointVisibility(
          base::BindRepeating(&NetworkScreen::SetQuickStartButtonVisibility,
                              weak_ptr_factory_.GetWeakPtr()));

  // The network screen is reused during QuickStart. Here we determine which
  // strings are shown on the screen accordingly.
  if (context()->quick_start_setup_ongoing) {
    ShowStepsWhenQuickStartOngoing();
  } else {
    // Shows the typical network list.
    view_->ShowScreenWithData({});
  }

  // Call Refresh() last. This could cause the NetworkScreen to exit, in which
  // case we don't want to access member variables after this point.
  Refresh();
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
      ExitQuickStartFlow(quick_start::QuickStartController::AbortFlowReason::
                             USER_CLICKED_BACK);
    } else {
      OnBackButtonClicked();
    }
  } else if (action_id == kUserActionCancelButtonClicked) {
    CHECK(context()->quick_start_setup_ongoing);
    ExitQuickStartFlow(quick_start::QuickStartController::AbortFlowReason::
                           USER_CLICKED_CANCEL);
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
      ExitQuickStartFlow(
          quick_start::QuickStartController::AbortFlowReason::ERROR);
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

    // Do not change the view if we are waiting for the network to stabilize.
    // The system will automatically transition to the next screen.
    if (waiting_for_quickstart_stabilization_period_) {
      quick_start::QS_LOG(INFO) << __func__ << "Waiting for screen change.";
      return;
    }

    // Show the standard 'Network List', but with custom strings notifying the
    // user that WiFi transfer has failed and that they should try again.
    if (view_) {
      view_->ShowScreenWithData(
          base::Value::Dict().Set("useQuickStartWiFiErrorStrings", true));
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
  waiting_for_quickstart_stabilization_period_ = false;
  UnsubscribeNetworkNotification();
  exit_callback_.Run(Result::CONNECTED);
}

void NetworkScreen::OnConnectionTimeout() {
  if (context()->quick_start_setup_ongoing) {
    ExitQuickStartFlow(
        quick_start::QuickStartController::AbortFlowReason::ERROR);
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
  const bool using_quickstart_wifi_cred =
      context() && context()->quick_start_setup_ongoing &&
      did_receive_quickstart_wifi_credentials_;

  // QuickStart case. Delay switching to the next screen by a couple of seconds
  // while waiting for the network to stabilize. This is also a UX request so
  // that users have enough time to see the WiFi transfer step. b/328677262
  if (is_connected && using_quickstart_wifi_cred) {
    if (waiting_for_quickstart_stabilization_period_) {
      return;
    }

    // Prevents posting multiple tasks, since this method might get triggered
    // multiple times.
    waiting_for_quickstart_stabilization_period_ = true;
    did_receive_quickstart_wifi_credentials_ = false;
    quick_start::QS_LOG(INFO) << "Connected. About to advance screens.";
    base::SingleThreadTaskRunner::GetCurrentDefault()->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&NetworkScreen::NotifyOnConnection,
                       weak_ptr_factory_.GetWeakPtr()),
        quickstart_stabilization_period_);
    return;
  }

  if (is_connected && continue_pressed_) {
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
  CHECK(!context()->quick_start_setup_ongoing);
  exit_callback_.Run(Result::QUICK_START);
}

void NetworkScreen::SetQuickStartButtonVisibility(bool visible) {
  if (!view_) {
    return;
  }

  view_->SetQuickStartEntryPointVisibility(visible);

  if (visible && !has_emitted_quick_start_visible) {
    has_emitted_quick_start_visible = true;
    quick_start::QuickStartMetrics::RecordEntryPointVisible(
        quick_start::QuickStartMetrics::EntryPoint::NETWORK_SCREEN);
  }
}

void NetworkScreen::ConfigureWifiNetwork(
    const quick_start::mojom::WifiCredentials& wifi_credentials) {
  quick_start::QS_LOG(INFO) << "Configuring WiFi...";
  network_id_ = base::UTF8ToUTF16(wifi_credentials.ssid);

  auto config = CreateNetworkConfig(wifi_credentials);
  remote_cros_network_config_->ConfigureNetwork(
      std::move(config), /*shared=*/true,
      base::BindOnce(&NetworkScreen::OnConfigureWifiNetworkResult,
                     weak_ptr_factory_.GetWeakPtr()));
}

void NetworkScreen::OnConfigureWifiNetworkResult(
    const std::optional<std::string>& network_guid,
    const std::string& error_message) {
  if (!network_guid.has_value() || !error_message.empty()) {
    LOG(ERROR) << "Configure network failed with  "
               << " network_guid: " << network_guid.value_or("none")
               << " and error_message: " << error_message;
    ExitQuickStartFlow(
        quick_start::QuickStartController::AbortFlowReason::ERROR);
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
    ExitQuickStartFlow(
        quick_start::QuickStartController::AbortFlowReason::ERROR);
    return;
  }
  WaitForConnection(network_id_);
}

void NetworkScreen::ExitQuickStartFlow(
    quick_start::QuickStartController::AbortFlowReason reason) {
  CHECK(context()->quick_start_setup_ongoing);
  auto* quick_start_controller = LoginDisplayHost::default_host()
                                     ->GetWizardController()
                                     ->quick_start_controller();
  quick_start_controller->DetachFrontend(this);
  const auto entry_point = quick_start_controller->GetExitPoint();
  quick_start_controller->AbortFlow(reason);

  // If we scheduled a call to switch to the next screen already, do not change
  // the UI.
  if (waiting_for_quickstart_stabilization_period_) {
    quick_start::QS_LOG(INFO) << "Waiting for network to stabilize.";
    return;
  }

  // If an error occurred, we show the network list with special strings telling
  // the user about the error, regardless of the entry point. If the user did
  // cancel the flow by themselves, no error messages are shown and the flow
  // goes back to the screen where it started.
  if (reason == quick_start::QuickStartController::AbortFlowReason::ERROR) {
    if (view_) {
      SetQuickStartButtonVisibility(/*visible=*/true);
      view_->ShowScreenWithData(
          base::Value::Dict().Set("useQuickStartWiFiErrorStrings", true));
    }
    return;
  }

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
    did_receive_quickstart_wifi_credentials_ = true;
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

  if (!first_ethernet_connection_) {
    return false;
  }

  if (!network_state_helper_->IsConnectedToEthernet()) {
    return false;
  }

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
