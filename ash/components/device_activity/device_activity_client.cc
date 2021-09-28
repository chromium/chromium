// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/device_activity/device_activity_client.h"

namespace ash {
namespace device_activity {

class DeviceActivityClient::NetworkObserver
    : public chromeos::NetworkStateHandlerObserver {
 public:
  NetworkObserver() = default;
  NetworkObserver(const NetworkObserver&) = delete;
  NetworkObserver& operator=(const NetworkObserver&) = delete;

  // Method gets called when the state of the default (primary)
  // network OR properties of the default network changes.
  void DefaultNetworkChanged(const NetworkState* network) override {
    OnNetworkChange(network);
  }

 protected:
  // TODO(hirthanan): Add callback to report actives after
  // synchronizing the system clock.
  void OnNetworkConnected() {}

  // TODO(hirthanan): Cancel all scheduled tasks.
  void OnNetworkDisconnected() {}

  // Handle network state changes to start device active reporting when the
  // network connects, and cancel reporting when the network disconnects.
  void OnNetworkChange(const NetworkState* network) {
    bool was_connected = network_connected_;
    network_connected_ = network && network->IsOnline();

    if (network_connected_ == was_connected)
      return;

    if (network_connected_)
      OnNetworkConnected();
    else
      OnNetworkDisconnected();
  }

 private:
  bool network_connected_ = false;
};

DeviceActivityClient::DeviceActivityClient(Trigger t,
                                           NetworkStateHandler* handler)
    : network_state_handler_(handler),
      network_observer_(std::make_unique<NetworkObserver>()) {
  network_state_handler_->AddObserver(network_observer_.get(), FROM_HERE);
  network_observer_->DefaultNetworkChanged(
      network_state_handler_->DefaultNetwork());
}

DeviceActivityClient::~DeviceActivityClient() {
  network_state_handler_->RemoveObserver(network_observer_.get(), FROM_HERE);
}

}  // namespace device_activity
}  // namespace ash
