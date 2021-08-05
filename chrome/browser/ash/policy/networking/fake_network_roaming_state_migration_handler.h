// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_NETWORKING_FAKE_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_
#define CHROME_BROWSER_ASH_POLICY_NETWORKING_FAKE_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_

#include "chrome/browser/ash/policy/networking/network_roaming_state_migration_handler.h"

namespace policy {

// Fake NetworkRoamingStateMigrationHandler implementation.
class FakeNetworkRoamingStateMigrationHandler
    : public NetworkRoamingStateMigrationHandler {
 public:
  FakeNetworkRoamingStateMigrationHandler();
  ~FakeNetworkRoamingStateMigrationHandler() override;

  using NetworkRoamingStateMigrationHandler::NotifyFoundCellularNetwork;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_NETWORKING_FAKE_NETWORK_ROAMING_STATE_MIGRATION_HANDLER_H_
