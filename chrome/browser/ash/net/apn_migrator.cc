// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/apn_migrator.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/network_config_service.h"
#include "base/values.h"
#include "chromeos/ash/components/login/login_state/login_state.h"
#include "chromeos/ash/components/network/managed_cellular_pref_handler.h"
#include "chromeos/ash/components/network/managed_network_configuration_handler.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_metadata_store.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_util.h"
#include "components/device_event_log/device_event_log.h"

namespace ash {

namespace {

void OnSetShillUserApnListSuccess() {}

void OnSetShillUserApnListFailure(const std::string& guid,
                                  const std::string& error_name) {
  NET_LOG(ERROR) << "ApnMigrator: Failed to update the user APN "
                    "list in Shill for network: "
                 << guid << ": [" << error_name << ']';
}

}  // namespace

ApnMigrator::ApnMigrator(
    ManagedCellularPrefHandler* managed_cellular_pref_handler,
    ManagedNetworkConfigurationHandler* network_configuration_handler,
    NetworkStateHandler* network_state_handler,
    NetworkMetadataStore* network_metadata_store)
    : managed_cellular_pref_handler_(managed_cellular_pref_handler),
      network_configuration_handler_(network_configuration_handler),
      network_state_handler_(network_state_handler),
      network_metadata_store_(network_metadata_store) {
  if (!NetworkHandler::IsInitialized()) {
    return;
  }
  // TODO(b/162365553): Only bind this lazily when CrosNetworkConfig is actually
  // used.
  ash::GetNetworkConfigService(
      remote_cros_network_config_.BindNewPipeAndPassReceiver());
  network_state_handler_observer_.Observe(network_state_handler_);
}

ApnMigrator::~ApnMigrator() = default;

void ApnMigrator::NetworkListChanged() {
  NetworkStateHandler::NetworkStateList network_list;
  network_state_handler_->GetVisibleNetworkListByType(
      NetworkTypePattern::Cellular(), &network_list);
  for (const NetworkState* network : network_list) {
    // Only attempt to migrate networks known by Shill.
    if (network->IsNonShillCellularNetwork()) {
      continue;
    }

    bool has_network_been_migrated =
        managed_cellular_pref_handler_->ContainsApnMigratedIccid(
            network->iccid());
    if (!ash::features::IsApnRevampEnabled()) {
      // If the network has been marked as migrated, but the ApnRevamp flag is
      // disabled, the flag was disabled after being enabled. Clear UserApnList
      // so that Shill knows to use legacy APN selection logic.
      if (has_network_been_migrated) {
        SetShillUserApnListForNetwork(*network, /*apn_list=*/nullptr);
      }
      continue;
    }

    if (!has_network_been_migrated) {
      NET_LOG(DEBUG) << "Network has not been migrated, attempting to migrate: "
                     << network->iccid();
      MigrateNetwork(*network);
      continue;
    }

    // The network has already been migrated. Send Shill the revamp APN list.
    if (const base::Value::List* custom_apn_list =
            network_metadata_store_->GetCustomApnList(network->guid())) {
      SetShillUserApnListForNetwork(*network, custom_apn_list);
      continue;
    }
    base::Value::List empty_user_apn_list;
    SetShillUserApnListForNetwork(*network, &empty_user_apn_list);
  }
}

void ApnMigrator::SetShillUserApnListForNetwork(
    const NetworkState& network,
    const base::Value::List* apn_list) {
  network_configuration_handler_->SetProperties(
      network.path(),
      chromeos::network_config::UserApnListToOnc(network.guid(), apn_list),
      base::BindOnce(&OnSetShillUserApnListSuccess),
      base::BindOnce(&OnSetShillUserApnListFailure, network.guid()));
}

void ApnMigrator::MigrateNetwork(const NetworkState& network) {
  DCHECK(ash::features::IsApnRevampEnabled());

  // Return early if the network is already in the process of being migrated.
  if (iccids_in_migration_.find(network.iccid()) !=
      iccids_in_migration_.end()) {
    NET_LOG(DEBUG) << "Attempting to migrate network that already has a "
                   << "migration in progress, returning early: "
                   << network.iccid();
    return;
  }
  DCHECK(!managed_cellular_pref_handler_->ContainsApnMigratedIccid(
      network.iccid()));

  // Get the pre-revamp APN list.
  const base::Value::List* custom_apn_list =
      network_metadata_store_->GetPreRevampCustomApnList(network.guid());

  // If the pre-revamp APN list is empty, set the revamp list as empty and
  // finish the migration.
  if (!custom_apn_list || custom_apn_list->empty()) {
    NET_LOG(EVENT) << "Pre-revamp APN list is empty, sending empty list to "
                      "Shill and marking as migrated: "
                   << network.iccid();
    base::Value::List empty_apn_list;
    SetShillUserApnListForNetwork(network, &empty_apn_list);
    managed_cellular_pref_handler_->AddApnMigratedIccid(network.iccid());
    return;
  }

  // If the pre-revamp APN list is non-empty, get the network's managed
  // properties, to be used for the migration heuristic. This call is
  // asynchronous; mark the ICCID as migrating so that the network won't
  // be attempted to be migrated again while these properties are being fetched.
  iccids_in_migration_.emplace(network.iccid());

  NET_LOG(EVENT) << "Fetching managed properties for network: "
                 << network.iccid();
  network_configuration_handler_->GetManagedProperties(
      LoginState::Get()->primary_user_hash(), network.path(),
      base::BindOnce(&ApnMigrator::OnGetManagedProperties,
                     weak_factory_.GetWeakPtr(), network.iccid(),
                     network.guid()));
}

void ApnMigrator::OnGetManagedProperties(
    std::string iccid,
    std::string guid,
    const std::string& service_path,
    absl::optional<base::Value::Dict> properties,
    absl::optional<std::string> error) {
  if (error.has_value()) {
    NET_LOG(ERROR) << "Error fetching managed properties for " << iccid
                   << ", error: " << error.value();
    iccids_in_migration_.erase(iccid);
    return;
  }

  if (!properties) {
    NET_LOG(ERROR) << "Error fetching managed properties for " << iccid;
    iccids_in_migration_.erase(iccid);
    return;
  }

  const NetworkState* network =
      network_state_handler_->GetNetworkStateFromGuid(guid);
  if (!network) {
    NET_LOG(ERROR) << "Network no longer exists: " << guid;
    iccids_in_migration_.erase(iccid);
    return;
  }

  // Get the pre-revamp APN list.
  const base::Value::List* custom_apn_list =
      network_metadata_store_->GetPreRevampCustomApnList(guid);

  // At this point, the pre-revamp APN list should not be empty. However, there
  // could be the case where the custom APN list was cleared during the
  // GetManagedProperties() call. If so, set the revamp list as empty and finish
  // the migration.
  if (!custom_apn_list || custom_apn_list->empty()) {
    NET_LOG(EVENT) << "Custom APN list cleared during GetManagedProperties() "
                   << "call, setting Shill with empty list for network: "
                   << guid;
    base::Value::List empty_apn_list;
    SetShillUserApnListForNetwork(*network, &empty_apn_list);
    managed_cellular_pref_handler_->AddApnMigratedIccid(iccid);
    iccids_in_migration_.erase(iccid);
    return;
  }

  if (network->IsManagedByPolicy()) {
    chromeos::network_config::mojom::ApnPropertiesPtr pre_revamp_custom_apn =
        chromeos::network_config::GetApnProperties(
            custom_apn_list->front().GetDict(),
            /*is_apn_revamp_enabled=*/false);
    const base::Value::Dict* cellular_dict =
        chromeos::network_config::GetDictionary(
            &properties.value(), ::onc::network_config::kCellular);
    chromeos::network_config::mojom::ManagedApnPropertiesPtr selected_apn =
        chromeos::network_config::GetManagedApnProperties(
            cellular_dict, ::onc::cellular::kAPN);
    if (selected_apn && pre_revamp_custom_apn->access_point_name ==
                            selected_apn->access_point_name->active_value) {
      NET_LOG(EVENT) << "Managed network's selected APN matches the saved "
                     << "custom APN, migrating APN: " << guid;
      // Ensure the network is enabled when it's migrated so that it's attempted
      // to be used by the new UI.
      pre_revamp_custom_apn->state =
          chromeos::network_config::mojom::ApnState::kEnabled;
      remote_cros_network_config_->CreateCustomApn(
          guid, std::move(pre_revamp_custom_apn));
    } else {
      NET_LOG(EVENT)
          << "Managed network's selected APN doesn't match the saved custom "
          << "APN, setting Shill with empty list for network: " << guid;
      base::Value::List empty_apn_list;
      SetShillUserApnListForNetwork(*network, &empty_apn_list);
    }
  } else {
    // TODO(b/162365553): Implement this case.
  }

  managed_cellular_pref_handler_->AddApnMigratedIccid(iccid);
  iccids_in_migration_.erase(iccid);
}

}  // namespace ash
