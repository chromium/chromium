// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LACROS_NET_NETWORK_CHANGE_MANAGER_BRIDGE_H_
#define CHROME_BROWSER_LACROS_NET_NETWORK_CHANGE_MANAGER_BRIDGE_H_

#include "chromeos/crosapi/mojom/network_change.mojom.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/network_change_notifier.h"
#include "services/network/public/mojom/network_change_manager.mojom.h"

namespace net {
class NetworkChangeNotifierPosix;
}

// Passes NetworkChange status given from Ash to NetworkChangeNotifier in
// browser process and NetworkChangeManager in NetworkService process.
// This class only runs on browser process.
class NetworkChangeManagerBridge
    : public crosapi::mojom::NetworkChangeObserver {
 public:
  NetworkChangeManagerBridge();

  NetworkChangeManagerBridge(const NetworkChangeManagerBridge&) = delete;
  NetworkChangeManagerBridge& operator=(const NetworkChangeManagerBridge&) =
      delete;

  ~NetworkChangeManagerBridge() override;

  // crosapi::mojom::NetworkChangeObserver
  void OnNetworkChanged(
      bool dns_changed,
      bool ip_address_changed,
      bool connection_type_changed,
      crosapi::mojom::ConnectionType new_connection_type,
      bool connection_subtype_changed,
      crosapi::mojom::ConnectionSubtype new_connection_subtype) override;

 private:
  void ConnectToNetworkChangeManager();
  void ReconnectToNetworkChangeManager();

  net::NetworkChangeNotifier::ConnectionType connection_type_ =
      net::NetworkChangeNotifier::CONNECTION_NONE;
  net::NetworkChangeNotifier::ConnectionSubtype connection_subtype_ =
      net::NetworkChangeNotifier::SUBTYPE_NONE;

  const raw_ptr<net::NetworkChangeNotifierPosix> network_change_notifier_;
  mojo::Remote<network::mojom::NetworkChangeManager> network_change_manager_;

  // Receives mojo messages from ash-chrome.
  mojo::Receiver<crosapi::mojom::NetworkChangeObserver> receiver_{this};
};

#endif  // CHROME_BROWSER_LACROS_NET_NETWORK_CHANGE_MANAGER_BRIDGE_H_
