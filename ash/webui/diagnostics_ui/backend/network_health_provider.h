// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
#define ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_

#include <map>
#include <string>
#include <vector>

#include "ash/webui/diagnostics_ui/mojom/network_health_provider.mojom.h"
#include "chromeos/services/network_config/public/mojom/cros_network_config.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace ash {
namespace diagnostics {
// Stores network state, managed properties, and an observer for a network.
struct NetworkProperties {
  explicit NetworkProperties(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state);
  ~NetworkProperties();
  chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state;
  chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties;
  mojo::Remote<mojom::NetworkStateObserver> observer;
};

using NetworkPropertiesMap = std::map<std::string, NetworkProperties>;

using DeviceMap =
    std::map<chromeos::network_config::mojom::NetworkType,
             chromeos::network_config::mojom::DeviceStatePropertiesPtr>;

class NetworkHealthProvider
    : public chromeos::network_config::mojom::CrosNetworkConfigObserver,
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
                      const std::string& guid) override;

  void BindInterface(
      mojo::PendingReceiver<mojom::NetworkHealthProvider> pending_receiver);

  // CrosNetworkConfigObserver
  void OnNetworkStateListChanged() override;
  void OnDeviceStateListChanged() override;
  void OnActiveNetworksChanged(
      std::vector<chromeos::network_config::mojom::NetworkStatePropertiesPtr>
          active_networks) override;
  void OnNetworkStateChanged(
      chromeos::network_config::mojom::NetworkStatePropertiesPtr network_state)
      override;
  void OnVpnProvidersChanged() override;
  void OnNetworkCertificatesChanged() override;

  std::vector<std::string> GetNetworkGuidList();

  const DeviceMap& GetDeviceTypeMapForTesting();

  const NetworkPropertiesMap& GetNetworkPropertiesMapForTesting();

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
      const std::string& guid,
      chromeos::network_config::mojom::ManagedPropertiesPtr managed_properties);

  // Gets ManagedProperties for a network |guid| from CrosNetworkConfig.
  void GetManagedPropertiesForNetwork(const std::string& guid);

  // Gets a list of network guids as well as the guid of the currently active
  // network (if one exists) and uses |network_list_observer_| to send the
  // result to each observer.
  void NotifyNetworkListObservers();

  // Creates a mojom::Network struct and sends it to the corresponding
  // network state observer.
  void NotifyNetworkStateObserver(const NetworkProperties& network_props);

  // Gets network state from CrosNetworkConfig.
  void GetNetworkState();

  // Gets device state from CrosNetworkConfig.
  void GetDeviceState();

  NetworkProperties& GetNetworkProperties(const std::string& guid);

  // Finds a matching device for a given network type.
  chromeos::network_config::mojom::DeviceStateProperties* GetMatchingDevice(
      chromeos::network_config::mojom::NetworkType type);

  // Map of networks that are active and of a supported
  // type (Ethernet, WiFi, Cellular).
  NetworkPropertiesMap network_properties_map_;

  // Maps device type to device properties, used to find corresponding device
  // for a network.
  DeviceMap device_type_map_;

  // Guid for the currently active network (if one exists).
  std::string active_guid_;

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

#endif  // ASH_WEBUI_DIAGNOSTICS_UI_BACKEND_NETWORK_HEALTH_PROVIDER_H_
