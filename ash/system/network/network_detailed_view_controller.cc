// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view_controller.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/bluetooth_config_service.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/network/network_utils.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/ash/components/network/network_connect.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {

namespace {

using ::base::UserMetricsAction;
using bluetooth_config::mojom::BluetoothSystemPropertiesPtr;
using bluetooth_config::mojom::BluetoothSystemState;
using ::chromeos::network_config::NetworkTypeMatchesType;
using ::chromeos::network_config::mojom::ActivationStateType;
using ::chromeos::network_config::mojom::CellularStateProperties;
using ::chromeos::network_config::mojom::ConnectionStateType;
using ::chromeos::network_config::mojom::DeviceStateType;
using ::chromeos::network_config::mojom::NetworkStateProperties;
using ::chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using ::chromeos::network_config::mojom::NetworkType;
using ::chromeos::network_config::mojom::PortalState;

bool IsSecondaryUser() {
  SessionControllerImpl* session_controller =
      Shell::Get()->session_controller();
  return session_controller->IsActiveUserSessionStarted() &&
         !session_controller->IsUserPrimary();
}

bool NetworkTypeIsConfigurable(NetworkType type) {
  switch (type) {
    case NetworkType::kVPN:
    case NetworkType::kWiFi:
      return true;
    case NetworkType::kAll:
    case NetworkType::kCellular:
    case NetworkType::kEthernet:
    case NetworkType::kMobile:
    case NetworkType::kTether:
    case NetworkType::kWireless:
      return false;
  }
  NOTREACHED();
}

bool IsNetworkBehindPortalOrProxy(PortalState portalState) {
  return portalState == PortalState::kPortal ||
         portalState == PortalState::kPortalSuspected;
}

bool IsNetworkConnectable(const NetworkStatePropertiesPtr& network_properties) {
  // The network must not already be connected to be able to be connected to.
  if (network_properties->connection_state !=
      ConnectionStateType::kNotConnected) {
    return false;
  }

  if (NetworkTypeMatchesType(network_properties->type,
                             NetworkType::kCellular)) {
    // Cellular networks must be activated, uninhibited, and have an unlocked
    // SIM to be able to be connected to.
    const CellularStateProperties* cellular =
        network_properties->type_state->get_cellular().get();

    if (cellular->activation_state == ActivationStateType::kNotActivated &&
        !cellular->eid.empty()) {
      return false;
    }

    if (cellular->activation_state == ActivationStateType::kActivated) {
      return true;
    }
  }

  // The network can be connected to if the network is connectable.
  if (network_properties->connectable) {
    return true;
  }

  // Network can be connected to if the active user is the primary user and the
  // network is configurable.
  if (!IsSecondaryUser() &&
      NetworkTypeIsConfigurable(network_properties->type)) {
    return true;
  }

  return false;
}

}  // namespace

NetworkDetailedViewController::NetworkDetailedViewController(
    UnifiedSystemTrayController* tray_controller)
    : model_(Shell::Get()->system_tray_model()->network_state_model()),
      detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  GetBluetoothConfigService(
      remote_cros_bluetooth_config_.BindNewPipeAndPassReceiver());
  remote_cros_bluetooth_config_->ObserveSystemProperties(
      cros_system_properties_observer_receiver_.BindNewPipeAndPassRemote());
}

NetworkDetailedViewController::~NetworkDetailedViewController() = default;

std::unique_ptr<views::View> NetworkDetailedViewController::CreateView() {
  DCHECK(!network_detailed_view_);
  std::unique_ptr<NetworkDetailedNetworkView> view =
      NetworkDetailedNetworkView::Factory::Create(detailed_view_delegate_.get(),
                                                  /*delegate=*/this);
  network_detailed_view_ = view.get();
  network_list_view_controller_ =
      NetworkListViewController::Factory::Create(view.get());

  // `view` is not a views::View, so we must GetAsView().
  return base::WrapUnique(view.release()->GetAsView());
}

std::u16string NetworkDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_NETWORK_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void NetworkDetailedViewController::OnNetworkListItemSelected(
    const NetworkStatePropertiesPtr& network) {
  if (Shell::Get()->session_controller()->login_status() ==
      LoginStatus::LOCKED) {
    return;
  }

  if (network) {
    // If the network is locked and is cellular show SIM unlock dialog in OS
    // Settings.
    if (network->type == NetworkType::kCellular &&
        network->type_state->get_cellular()->sim_locked) {
      if (!Shell::Get()->session_controller()->ShouldEnableSettings()) {
        return;
      }
      // It is not possible to unlock the carrier locked device by entering the
      // pin on UI as unlock flow is triggered by simLock server
      if (network->type_state->get_cellular()->sim_lock_type == "network-pin") {
        return;
      }
      Shell::Get()->system_tray_model()->client()->ShowSettingsSimUnlock();
      return;
    }

    // If user is logged in, the network is connected, and the network is in a
    // portal or proxy state, the user is shown the portal signin. We do not
    // show portal sign in for user not logged in because it is the only way for
    // the user to get to the network details page.
    if (Shell::Get()->session_controller()->login_status() !=
            LoginStatus::NOT_LOGGED_IN &&
        chromeos::network_config::StateIsConnected(network->connection_state) &&
        IsNetworkBehindPortalOrProxy(network->portal_state)) {
      NetworkConnect::Get()->ShowPortalSignin(
          network->guid, NetworkConnect::Source::kQuickSettings);
      return;
    }

    if (IsNetworkConnectable(network)) {
      base::RecordAction(
          UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      NetworkConnect::Get()->ConnectToNetworkId(network->guid);
      return;
    }
  }

  // If the network is no longer available or not connectable or configurable,
  // show the Settings UI.
  base::RecordAction(UserMetricsAction("StatusArea_Network_ConnectionDetails"));
  Shell::Get()->system_tray_model()->client()->ShowNetworkSettings(
      network ? network->guid : std::string());
}

void NetworkDetailedViewController::OnMobileToggleClicked(bool new_state) {
  const DeviceStateType cellular_state =
      model_->GetDeviceState(NetworkType::kCellular);

  // When Cellular is available, the toggle controls Cellular enabled state.
  if (cellular_state != DeviceStateType::kUnavailable) {
    model_->SetNetworkTypeEnabledState(NetworkType::kCellular, new_state);
    return;
  }

  if (features::IsInstantHotspotRebrandEnabled()) {
    return;
  }

  const DeviceStateType tether_state =
      model_->GetDeviceState(NetworkType::kTether);

  DCHECK(tether_state != DeviceStateType::kUnavailable);

  // If Tether is available but uninitialized, we expect Bluetooth to be off.
  // Enable Bluetooth so that Tether will be initialized.
  if (tether_state == DeviceStateType::kUninitialized) {
    if (new_state &&
        (bluetooth_system_state_ == BluetoothSystemState::kDisabled ||
         bluetooth_system_state_ == BluetoothSystemState::kDisabling)) {
      remote_cros_bluetooth_config_->SetBluetoothEnabledState(true);
      waiting_to_initialize_bluetooth_ = true;
    }
    return;
  }

  // Otherwise the toggle controls the Tether enabled state.
  model_->SetNetworkTypeEnabledState(NetworkType::kTether, new_state);
}

void NetworkDetailedViewController::OnWifiToggleClicked(bool new_state) {
  model_->SetNetworkTypeEnabledState(NetworkType::kWiFi, new_state);
}

void NetworkDetailedViewController::OnPropertiesUpdated(
    BluetoothSystemPropertiesPtr properties) {
  bluetooth_system_state_ = properties->system_state;

  // We enabled Bluetooth so Tether is now initialized, but it was not
  // enabled so enable it.
  if (waiting_to_initialize_bluetooth_ &&
      bluetooth_system_state_ == BluetoothSystemState::kEnabled) {
    waiting_to_initialize_bluetooth_ = false;
    model_->SetNetworkTypeEnabledState(NetworkType::kTether,
                                       /*enabled=*/true);
  }
}

void NetworkDetailedViewController::ShutDown() {
  network_list_view_controller_.reset();
}

}  // namespace ash
