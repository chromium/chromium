// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/network/network_detailed_view_controller.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/system_tray_client.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/strings/grit/ash_strings.h"
#include "ash/system/machine_learning/user_settings_event_logger.h"
#include "ash/system/model/system_tray_model.h"
#include "ash/system/network/network_detailed_network_view.h"
#include "ash/system/network/network_list_view_controller.h"
#include "ash/system/network/tray_network_state_model.h"
#include "ash/system/tray/detailed_view_delegate.h"
#include "base/metrics/user_metrics.h"
#include "chromeos/network/network_connect.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/view.h"

namespace ash {
namespace {

using base::UserMetricsAction;

using chromeos::network_config::NetworkTypeMatchesType;

using chromeos::network_config::mojom::ActivationStateType;
using chromeos::network_config::mojom::CellularStateProperties;
using chromeos::network_config::mojom::ConnectionStateType;
using chromeos::network_config::mojom::NetworkStateProperties;
using chromeos::network_config::mojom::NetworkStatePropertiesPtr;
using chromeos::network_config::mojom::NetworkType;

void LogUserNetworkEvent(const NetworkStateProperties& network) {
  auto* const logger = ml::UserSettingsEventLogger::Get();
  if (logger) {
    logger->LogNetworkUkmEvent(network);
  }
}

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
  return false;
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
    : detailed_view_delegate_(
          std::make_unique<DetailedViewDelegate>(tray_controller)) {
  DCHECK(ash::features::IsQuickSettingsNetworkRevampEnabled());
}

NetworkDetailedViewController::~NetworkDetailedViewController() = default;

views::View* NetworkDetailedViewController::CreateView() {
  DCHECK(!network_detailed_view_);
  std::unique_ptr<NetworkDetailedNetworkView> view =
      NetworkDetailedNetworkView::Factory::Create(detailed_view_delegate_.get(),
                                                  /*delegate=*/this);
  network_detailed_view_ = view.get();
  network_list_view_controller_ =
      NetworkListViewController::Factory::Create(view.get());

  // We are expected to return an unowned pointer that the caller is responsible
  // for deleting.
  return view.release()->GetAsView();
}

std::u16string NetworkDetailedViewController::GetAccessibleName() const {
  return l10n_util::GetStringUTF16(
      IDS_ASH_QUICK_SETTINGS_BUBBLE_NETWORK_SETTINGS_ACCESSIBLE_DESCRIPTION);
}

void NetworkDetailedViewController::OnNetworkListItemSelected(
    const NetworkStatePropertiesPtr& network) {
  if (Shell::Get()->session_controller()->login_status() == LoginStatus::LOCKED)
    return;

  if (network) {
    // If the network is locked and is cellular show SIM unlock dialog in OS
    // Settings.
    if (network->type == NetworkType::kCellular &&
        network->type_state->get_cellular()->sim_locked) {
      if (!Shell::Get()->session_controller()->ShouldEnableSettings()) {
        return;
      }
      Shell::Get()->system_tray_model()->client()->ShowSettingsSimUnlock();
      return;
    }

    if (IsNetworkConnectable(network)) {
      base::RecordAction(
          UserMetricsAction("StatusArea_Network_ConnectConfigured"));
      LogUserNetworkEvent(*network.get());
      chromeos::NetworkConnect::Get()->ConnectToNetworkId(network->guid);
      return;
    }
  }

  // If the network is no longer available or not connectable or configurable,
  // show the Settings UI.
  base::RecordAction(UserMetricsAction("StatusArea_Network_ConnectionDetails"));
  Shell::Get()->system_tray_model()->client()->ShowNetworkSettings(
      network ? network->guid : std::string());
}

void NetworkDetailedViewController::OnMobileToggleClicked(bool new_state) {}

void NetworkDetailedViewController::OnWifiToggleClicked(bool new_state) {
  Shell::Get()
      ->system_tray_model()
      ->network_state_model()
      ->SetNetworkTypeEnabledState(NetworkType::kWiFi, new_state);
}

}  // namespace ash
