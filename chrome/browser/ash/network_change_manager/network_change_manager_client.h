// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_NETWORK_CHANGE_MANAGER_NETWORK_CHANGE_MANAGER_CLIENT_H_
#define CHROME_BROWSER_ASH_NETWORK_CHANGE_MANAGER_NETWORK_CHANGE_MANAGER_CLIENT_H_

#include <string>

#include "base/gtest_prod_util.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chromeos/ash/components/network/network_state_handler_observer.h"
#include "chromeos/dbus/power/power_manager_client.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace net {
class NetworkChangeNotifierPassive;
}  // namespace net

namespace ash {

class NetworkStateHandler;

// This class listens to Shill for network change events and notifies both
// the local NetworkChangeNotifierPassive, and the network service via
// the NetworkChangeManager if the network service is enabled.
class NetworkChangeManagerClient
    : public chromeos::PowerManagerClient::Observer,
      public NetworkStateHandlerObserver {
 public:
  explicit NetworkChangeManagerClient(
      net::NetworkChangeNotifierPassive* network_change_notifier);

  NetworkChangeManagerClient(const NetworkChangeManagerClient&) = delete;
  NetworkChangeManagerClient& operator=(const NetworkChangeManagerClient&) =
      delete;

  ~NetworkChangeManagerClient() override;

  // Returns NetworkChangeManagerClient instance.
  static NetworkChangeManagerClient* GetInstance();

  // PowerManagerClient::Observer overrides.
  void SuspendDone(base::TimeDelta sleep_duration) override;

  // NetworkStateHandlerObserver overrides.
  void DefaultNetworkChanged(const NetworkState* default_network) override;

 private:
  friend class NetworkChangeManagerClientUpdateTest;
  FRIEND_TEST_ALL_PREFIXES(NetworkChangeManagerClientTest,
                           ConnectionTypeFromShill);
  FRIEND_TEST_ALL_PREFIXES(NetworkChangeManagerClientTest,
                           NetworkChangeNotifierConnectionTypeUpdated);

  void ConnectToNetworkChangeManager();
  void ReconnectToNetworkChangeManager();

  // Updates the notifier state based on a default network update.
  // |connection_type_changed| is set to true if we must report a connection
  // type change.
  // |ip_address_changed| is set to true if we must report an IP address change.
  // |dns_changed| is set to true if we must report a DNS config change.
  // |connection_subtype_changed| is set to true if we must report a connection
  // subtype change.
  void UpdateState(const NetworkState* default_network,
                   bool* dns_changed,
                   bool* ip_address_changed,
                   bool* connection_type_changed,
                   bool* connection_subtype_changed);

  // Notifies NetworkChangeNotifier, NetworkChangeManager and
  // crosapi::mojom::NetworkChangeObserver.
  void NotifyObservers(
      bool dns_changed,
      bool ip_address_changed,
      bool connection_type_changed,
      net::NetworkChangeNotifier::ConnectionType connection_type,
      bool connection_subtype_changed,
      net::NetworkChangeNotifier::ConnectionSubtype connection_subtype);

  // Maps the shill network type and technology to its NetworkChangeNotifier
  // equivalent.
  //
  // This is a static member function for testing.
  static net::NetworkChangeNotifier::ConnectionType ConnectionTypeFromShill(
      const std::string& type,
      const std::string& technology);

  // Maps the shill network type and technology to its NetworkChangeNotifier
  // subtype equivalent.
  //
  // This is a static member function for testing.
  static net::NetworkChangeNotifier::ConnectionSubtype GetConnectionSubtype(
      const std::string& type,
      const std::string& technology);

  net::NetworkChangeNotifier::ConnectionType connection_type_;
  net::NetworkChangeNotifier::ConnectionSubtype connection_subtype_;
  // IP address for the current default network.
  std::string ip_address_;
  // DNS servers for the current default network.
  std::string dns_servers_;
  // Service path for the current default network.
  std::string service_path_;

  base::ScopedObservation<NetworkStateHandler, NetworkStateHandlerObserver>
      network_state_handler_observer_{this};

  raw_ptr<net::NetworkChangeNotifierPassive> network_change_notifier_;
  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager_;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NETWORK_CHANGE_MANAGER_NETWORK_CHANGE_MANAGER_CLIENT_H_
