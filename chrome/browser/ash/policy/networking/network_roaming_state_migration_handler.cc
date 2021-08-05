// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler.h"

namespace policy {

NetworkRoamingStateMigrationHandler::NetworkRoamingStateMigrationHandler() =
    default;

NetworkRoamingStateMigrationHandler::~NetworkRoamingStateMigrationHandler() =
    default;

void NetworkRoamingStateMigrationHandler::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void NetworkRoamingStateMigrationHandler::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void NetworkRoamingStateMigrationHandler::NotifyFoundCellularNetwork(
    bool roaming_enabled) {
  for (auto& observer : observers_) {
    observer.OnFoundCellularNetwork(roaming_enabled);
  }
}

}  // namespace policy
