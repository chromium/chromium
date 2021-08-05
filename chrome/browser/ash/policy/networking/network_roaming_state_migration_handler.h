// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_

#include "base/observer_list.h"
#include "base/observer_list_types.h"

namespace policy {

// This class is responsible for clearing the Service.AllowRoaming Shill
// property of cellular networks, and notifying observers of whether cellular
// roaming was enabled for each network handled. This class is only enabled when
// per-network cellular roaming is disabled.
// TODO(crbug.com/1232818): Remove when per-network cellular roaming is fully
// launched.
class NetworkRoamingStateMigrationHandler {
 public:
  class Observer : public base::CheckedObserver {
   public:
    ~Observer() override = default;

    virtual void OnFoundCellularNetwork(bool roaming_enabled) = 0;
  };

  virtual ~NetworkRoamingStateMigrationHandler();

  // Allows clients and register and de-register themselves.
  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  NetworkRoamingStateMigrationHandler();

  // Notifies all observers that a cellular network was found, and whether or
  // not roaming was enabled for the network.
  void NotifyFoundCellularNetwork(bool roaming_enabled);

 private:
  base::ObserverList<Observer, /*check_empty=*/true> observers_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_
