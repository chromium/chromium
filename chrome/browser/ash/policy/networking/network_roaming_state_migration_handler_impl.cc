// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler_impl.h"

#include "ash/constants/ash_features.h"
#include "base/callback_helpers.h"
#include "base/feature_list.h"
#include "chromeos/dbus/shill/shill_service_client.h"
#include "chromeos/network/network_handler.h"
#include "chromeos/network/network_handler_callbacks.h"
#include "chromeos/network/network_state_handler.h"
#include "chromeos/network/network_type_pattern.h"
#include "components/device_event_log/device_event_log.h"
#include "dbus/object_path.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace policy {

NetworkRoamingStateMigrationHandlerImpl::
    NetworkRoamingStateMigrationHandlerImpl() {
  DCHECK(!base::FeatureList::IsEnabled(
      ash::features::kCellularAllowPerNetworkRoaming));
  if (chromeos::NetworkHandler::IsInitialized()) {
    network_state_handler_ =
        chromeos::NetworkHandler::Get()->network_state_handler();
    network_state_handler_->AddObserver(this, FROM_HERE);
  }
}

NetworkRoamingStateMigrationHandlerImpl::
    ~NetworkRoamingStateMigrationHandlerImpl() {
  if (network_state_handler_) {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
  }
}

void NetworkRoamingStateMigrationHandlerImpl::NetworkListChanged() {
  DCHECK(network_state_handler_);

  chromeos::NetworkStateHandler::NetworkStateList network_state_list;
  network_state_handler_->GetNetworkListByType(
      chromeos::NetworkTypePattern::Cellular(), /*configured_only=*/false,
      /*visible_only=*/false, /*limit=*/0, &network_state_list);

  for (const chromeos::NetworkState* network_state : network_state_list) {
    if (network_state->IsNonShillCellularNetwork() ||
        processed_cellular_guids_.contains(network_state->guid())) {
      continue;
    }
    NET_LOG(USER) << "Service.ClearProperty: "
                  << shill::kCellularAllowRoamingProperty;

    ash::ShillServiceClient::Get()->ClearProperty(
        dbus::ObjectPath(network_state->path()),
        shill::kCellularAllowRoamingProperty, base::DoNothing(),
        base::BindOnce(
            &ash::network_handler::ShillErrorCallbackFunction,
            "NetworkRoamingStateMigrationHandlerImpl.ClearProperty Failed",
            network_state->path(), ash::network_handler::ErrorCallback()));
    processed_cellular_guids_.insert(network_state->guid());

    NotifyFoundCellularNetwork(network_state->allow_roaming());
  }
}

void NetworkRoamingStateMigrationHandlerImpl::OnShuttingDown() {
  if (network_state_handler_) {
    network_state_handler_->RemoveObserver(this, FROM_HERE);
    network_state_handler_ = nullptr;
  }
}

}  // namespace policy
