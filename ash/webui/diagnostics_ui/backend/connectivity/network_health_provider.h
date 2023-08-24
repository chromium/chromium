// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CONNECTIVITY_NETWORK_HEALTH_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CONNECTIVITY_NETWORK_HEALTH_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "chromeos/services/network_config/public/cpp/cros_network_config_observer.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace diagnostics {

struct NetworkObserverInfo {
  NetworkObserverInfo();
  NetworkObserverInfo(NetworkObserverInfo&&);
  NetworkObserverInfo& operator=(NetworkObserverInfo&&);
  ~NetworkObserverInfo();
  std::string network_guid;
  mojom::NetworkPtr network;
  mojo::Remote<mojom::NetworkStateObserver> observer;
  chromeos::network_config::mojom::DeviceStateType device_state;
};

class NetworkHealthProvider
    : public chromeos::network_config::CrosNetworkConfigObserver,
      public mojom::NetworkHealthProvider {
 public:
  NetworkHealthProvider();

  NetworkHealthProvider(const NetworkHealthProvider&) = delete;
  NetworkHealthProvider& operator=(const NetworkHealthProvider&) = delete;

  ~NetworkHealthProvider() override;

  // mojom::NetworkHealthProvider
  void ObserveNetworkList(
      mojo::PendingRemote<mojom::NetworkListObserver> observer) override;

  void ObserveNetwork(mojo::PendingRemote<mojom::NetworkStateObserver> observer,
                      const std::string& observer_guid) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::NetworkHealthProvider> pending_receiver);

  // CrosNetworkConfigObserver
  void OnDeviceStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;

  // Returns the list of observer guids. Each guid corresponds to one network
  // interface. Additionally, updates the currently |active_guid_| to the first
  // active network interface, if one exists.
  std::vector<std::string> GetObserverGuidsAndUpdateActiveGuid();

 private:
  // Handler for receiving a list of active networks.
  void OnActiveNetworkStateListReceived(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          networks);

  // Handler for receiving a list of devices.
  void OnDeviceStateListReceived(
      std::vector<chromeos::network_config::mojom::DeviceStatePropertiesPtr>
          devices);

  // Handler for receiving managed properties for a network.
  void OnManagedPropertiesReceived(
      const std::string& observer_guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Gets ManagedProperties for a network |guid| from CrosNetworkConfig.
  void GetManagedPropertiesForNetwork(const std::string& network_guid,
                                      const std::string& observer_guid);

  // Notifies observers registered with ObserveNetworkList() the current list
  // of observer guids, and which one is active (if any).
  void NotifyNetworkListObservers();

  // Notifies an observer for a specific network registered with
  // ObserverNetwork() that there was a state change.
  void NotifyNetworkStateObserver(const NetworkObserverInfo& network_props);

  // Requests a callback with the list of active network states from
  // CrosNetworkConfig to OnActiveNetworksChanged().
  void GetActiveNetworkState();

  // Requests a callback with the list of device states from CrosNetworkConfig
  // to OnDeviceStateListChanged().
  void GetDeviceState();

  // Adds a net network to |networks_|. This is called
  std::string AddNewNetwork(
      const chromeos::network_config::mojom::DeviceStatePropertiesPtr& device);

  // Looks up a network in |networks_|. When |must_match_existing_guid| is true
  // a network will only match if the backend network guid matches. This is to
  // perform state updates to already known networks. When false, the network
  // will match to a device/interface which allows rebinding a new network
  // to a device/interface.
  NetworkObserverInfo* LookupNetwork(
      const chromeos::network_config::mojom::NetworkStatePropertiesPtr& network,
      bool must_match_existing_guid);

  // Performs a lookup, and updates the matching network (if any). The value
  // of |must_match_existing_guid| is describe in LookupNetwork().
  void UpdateMatchingNetwork(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network,
      bool must_match_existing_guid);

  mojom::NetworkState GetNetworkStateForGuid(const std::string& guid);

  // Guid for the currently active network (if one exists). This guid will
  // be present in |networks_|.
  std::string active_guid_;

  // A map from an observer guid, to a struct that contains network
  // information, the backends network guid, and a mojo remote for the
  // observer. Effectively each entry corresponds to a network interface
  // (or device in the backend API), however the network info is populated as
  // the aggregation of the device, and the network properties of any network
  // connected on that interface.
  base::flat_map<std::string, NetworkObserverInfo> networks_;

  // Remote for sending requests to the CrosNetworkConfig service.
  mojo::Remote<chromeos::network_config::mojom::CrosNetworkConfig>
      remote_cros_network_config_;

  // Receiver for the CrosNetworkConfigObserver events.
  mojo::Receiver<chromeos::network_config::mojom::CrosNetworkConfigObserver>
      cros_network_config_observer_receiver_{this};

  // Remotes for tracking observers that will be notified of changes to the
  // list of active networks.
  mojo::RemoteSet<mojom::NetworkListObserver> network_list_observers_;

  mojo::Receiver<mojom::NetworkHealthProvider> receiver_{this};
};

}  // namespace diagnostics
}  // namespace ash

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_CONNECTIVITY_NETWORK_HEALTH_PROVIDER_H_
