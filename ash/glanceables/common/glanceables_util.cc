// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/glanceables/common/glanceables_util.h"

#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state_handler.h"

namespace ash::glanceables_util {
namespace {

// A global flag for tests to manually set the connection of the network.
std::optional<bool> g_is_network_connected_for_test = std::nullopt;

}  // namespace

bool IsNetworkConnected() {
  if (g_is_network_connected_for_test.has_value()) {
    return g_is_network_connected_for_test.value();
  }

  const auto* const network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();

  return network && network->IsConnectedState();
}

void SetIsNetworkConnectedForTest(bool connected) {
  g_is_network_connected_for_test = connected;
}

}  // namespace ash::glanceables_util
