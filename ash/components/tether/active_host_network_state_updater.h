// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
#define ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_

#include "ash/components/tether/active_host.h"
#include "base/memory/weak_ptr.h"
// TODO(https://crbug.com/1164001): move to forward declaration
#include "chromeos/network/network_state_handler.h"

namespace ash {

namespace tether {

// Observes changes to the status of the active host, and relays these updates
// to the networking stack.
class ActiveHostNetworkStateUpdater final : public ActiveHost::Observer {
 public:
  ActiveHostNetworkStateUpdater(ActiveHost* active_host,
                                NetworkStateHandler* network_state_handler);

  ActiveHostNetworkStateUpdater(const ActiveHostNetworkStateUpdater&) = delete;
  ActiveHostNetworkStateUpdater& operator=(
      const ActiveHostNetworkStateUpdater&) = delete;

  ~ActiveHostNetworkStateUpdater();

  // ActiveHost::Observer:
  void OnActiveHostChanged(
      const ActiveHost::ActiveHostChangeInfo& change_info) override;

 private:
  ActiveHost* active_host_;
  NetworkStateHandler* network_state_handler_;
};

}  // namespace tether

}  // namespace ash

#endif  // ASH_COMPONENTS_TETHER_ACTIVE_HOST_NETWORK_STATE_UPDATER_H_
