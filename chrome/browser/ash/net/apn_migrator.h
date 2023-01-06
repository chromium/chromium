// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_
#define CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_

#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace ash {

class ManagedCellularPrefHandler;
class ManagedNetworkConfigurationHandler;
class NetworkMetadataStore;
class NetworkStateHandler;

// Handles migrating cellular networks' Access Point Names from the pre-revamp
// format to the revamped format (see go/launch/4210741) the first time each
// network is discovered with the kApnRevamp flag enabled.
class COMPONENT_EXPORT(CHROMEOS_NETWORK) ApnMigrator
    : public NetworkStateHandlerObserver {
 public:
  ApnMigrator(
      ManagedCellularPrefHandler* managed_cellular_pref_handler,
      ManagedNetworkConfigurationHandler* managed_network_configuration_handler,
      NetworkStateHandler* network_state_handler,
      NetworkMetadataStore* network_metadata_store);
  ApnMigrator() = delete;
  ApnMigrator(const ApnMigrator&) = delete;
  ApnMigrator& operator=(const ApnMigrator&) = delete;
  ~ApnMigrator() override;

 private:
  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  // Creates an ONC configuration object for the Shill property user_apn_list
  // containing |apn_list|, and applies it for the cellular |network|.
  void SetShillUserApnListForNetwork(const NetworkState& network,
                                     const base::Value::List* apn_list);

  // Migrate the |network|'s custom APNs to the APN Revamp feature. If the
  // migration requires the network's managed properties, this function will
  // invoke an async call, and mark the network as "in migration".
  void MigrateNetwork(const NetworkState& network);

  // Finishes the migration process for networks that require managed properties
  // fields.
  void OnGetManagedProperties(std::string iccid,
                              const std::string& service_path,
                              absl::optional<base::Value> properties,
                              absl::optional<std::string> error);

  base::flat_set<std::string> iccids_in_migration_;

  ManagedCellularPrefHandler* managed_cellular_pref_handler_ = nullptr;
  ManagedNetworkConfigurationHandler* network_configuration_handler_ = nullptr;
  NetworkStateHandler* network_state_handler_ = nullptr;
  NetworkMetadataStore* network_metadata_store_ = nullptr;

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  base::WeakPtrFactory<ApnMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_
