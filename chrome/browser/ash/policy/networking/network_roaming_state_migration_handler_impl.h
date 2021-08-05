// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_IMPL_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_IMPL_H_

#include <string>

#include "base/containers/flat_set.h"
#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler.h"
#include "chromeos/network/network_state_handler_observer.h"

namespace chromeos {
class NetworkStateHandler;
}  // namespace chromeos

namespace policy {

// AdapterStateController implementation which uses NetworkStateHandlerObserver.
class NetworkRoamingStateMigrationHandlerImpl
    : public chromeos::NetworkStateHandlerObserver,
      public NetworkRoamingStateMigrationHandler {
 public:
  NetworkRoamingStateMigrationHandlerImpl();
  ~NetworkRoamingStateMigrationHandlerImpl() override;

 private:
  // NetworkStateHandlerObserver:
  void NetworkListChanged() override;
  void OnShuttingDown() override;

  // The cellular networks that have already been processed.
  base::flat_set<std::string> processed_cellular_guids_;

  chromeos::NetworkStateHandler* network_state_handler_ = nullptr;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_IMPL_H_
