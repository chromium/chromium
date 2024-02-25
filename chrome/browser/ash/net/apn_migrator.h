// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_
#define CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_

#include "base/containers/flat_set.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
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
      NetworkStateHandler* network_state_handler);
  ApnMigrator() = delete;
  ApnMigrator(const ApnMigrator&) = delete;
  ApnMigrator& operator=(const ApnMigrator&) = delete;
  ~ApnMigrator() override;

 private:
  friend class ApnMigratorTest;

  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;

  void OnClearPropertiesSuccess(const std::string iccid);
  void OnClearPropertiesFailure(const std::string iccid,
                                const std::string guid,
                                const std::string& error_name);

  // Creates an ONC configuration object for the custom APN list Shill property
  // containing |apn_list|, and applies it for the cellular |network|.
  void SetShillCustomApnListForNetwork(const NetworkState& network,
                                       const base::Value::List* apn_list);

  void OnSetShillCustomApnListSuccess(const std::string iccid);
  void OnSetShillCustomApnListFailure(const std::string iccid,
                                      const std::string guid,
                                      const std::string& error_name);

  // Migrate the |network|'s custom APNs to the APN Revamp feature. If the
  // migration requires the network's managed properties, this function will
  // invoke an async call, and mark the network as "in migration".
  void MigrateNetwork(const NetworkState& network);

  // Finishes the migration process for networks that require managed properties
  // fields.
  void OnGetManagedProperties(std::string iccid,
                              std::string guid,
                              const std::string& service_path,
                              std::optional<base::Value::Dict> properties,
                              std::optional<std::string> error);

  // Helper func that creates the |default_apn| before creating the
  // |attach_apn|. If |can_use_default_apn_as_attach| is true, the |default_apn|
  // will be given the capability to attach as well.
  void CreateDefaultThenAttachCustomApns(
      chromeos::network_config::mojom::ApnPropertiesPtr attach_apn,
      chromeos::network_config::mojom::ApnPropertiesPtr default_apn,
      bool can_use_default_apn_as_attach,
      const std::string& guid,
      const std::string& iccid);

  void CreateCustomApn(const std::string& iccid,
                       const std::string& network_guid,
                       chromeos::network_config::mojom::ApnPropertiesPtr apn,
                       std::optional<base::OnceCallback<void(bool)>>
                           success_callback = std::nullopt);

  void CompleteMigrationAttempt(const std::string& iccid, bool success);

  NetworkMetadataStore* GetNetworkMetadataStore();

  void set_network_metadata_store_for_testing(
      NetworkMetadataStore* network_metadata_store_for_testing) {
    network_metadata_store_for_testing_ = network_metadata_store_for_testing;
  }

  // ICCIDs that are currently being migrated.
  base::flat_set<std::string> iccids_in_migration_;

  // ICCIDs of networks that have been configured in shill with the appropriate
  // CustomAPNList. Networks must be updated in shill with the CustomAPNList
  // property each time the revamp flag is toggled.
  base::flat_set<std::string> shill_updated_iccids_;

  raw_ptr<ManagedCellularPrefHandler> managed_cellular_pref_handler_ = nullptr;
  raw_ptr<ManagedNetworkConfigurationHandler> network_configuration_handler_ =
      nullptr;
  raw_ptr<NetworkStateHandler> network_state_handler_ = nullptr;

  // NetworkMetadataStore may be created and destroyed multiple times
  // in ApnMigrator's lifetime, so a reference to NetworkMetadataStore
  // should not be held. See http://b/285014794#comment19 for more info.
  // Note that this should only be non-nullptr in unit tests.
  raw_ptr<NetworkMetadataStore> network_metadata_store_for_testing_ = nullptr;

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};
  base::WeakPtrFactory<ApnMigrator> weak_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_APN_MIGRATOR_H_
