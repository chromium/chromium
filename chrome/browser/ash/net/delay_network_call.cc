// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/delay_network_call.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultRetryDelay = base::Seconds(3);

bool delay_network_calls_for_testing = false;

bool IsOnline(const NetworkState* default_network) {
  if (default_network->IsOnline()) {
    return true;
  }
  DVLOG(1) << "DelayNetworkCall: Not online. Connection state for "
           << default_network->name() << " = "
           << default_network->connection_state();
  return false;
}

}  // namespace

bool AreNetworkCallsDelayed() {
  if (delay_network_calls_for_testing) {
    return true;
  }

  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network) {
    DVLOG(1) << "DelayNetworkCall: No default network.";
    return true;
  }

  if (const std::string default_connection_state =
          default_network->connection_state();
      !NetworkState::StateIsConnected(default_connection_state)) {
    DVLOG(1) << "DelayNetworkCall: " << "Default network: "
             << default_network->name()
             << " State: " << default_connection_state;
    return true;
  }

  if (!IsOnline(default_network)) {
    return true;
  }

  return false;
}

void DelayNetworkCall(base::OnceClosure callback) {
  DelayNetworkCallWithCustomDelay(std::move(callback), kDefaultRetryDelay);
}

void DelayNetworkCallWithCustomDelay(base::OnceClosure callback,
                                     base::TimeDelta retry_delay) {
  if (AreNetworkCallsDelayed()) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DelayNetworkCallWithCustomDelay, std::move(callback),
                       retry_delay),
        retry_delay);
    return;
  }
  content::GetUIThreadTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(std::move(callback)));
}

void SetDelayNetworkCallsForTesting(bool delay_network_calls) {
  delay_network_calls_for_testing = delay_network_calls;
}

}  // namespace ash
