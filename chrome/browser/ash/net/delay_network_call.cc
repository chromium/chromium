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
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {

namespace {

constexpr base::TimeDelta kDefaultRetryDelay = base::Seconds(3);

bool IsCaptivePortal(const NetworkState* default_network) {
  if (!network_portal_detector::IsInitialized()) {
    // Network portal detector is not initialized yet so we can't reliably
    // detect network portals. We will optimistically return false here,
    // assuming that a network portal doesn't exist.
    return false;
  }

  if (const NetworkPortalDetector::CaptivePortalStatus status =
          network_portal_detector::GetInstance()->GetCaptivePortalStatus();
      status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
    DVLOG(1) << "DelayNetworkCall: Captive portal status for "
             << default_network->name() << ": "
             << NetworkPortalDetector::CaptivePortalStatusString(status);
    return true;
  }

  return false;
}

}  // namespace

bool AreNetworkCallsDelayed() {
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network) {
    DVLOG(1) << "DelayNetworkCall: No default network.";
    return true;
  }

  if (const std::string default_connection_state =
          default_network->connection_state();
      !NetworkState::StateIsConnected(default_connection_state)) {
    DVLOG(1) << "DelayNetworkCall: "
             << "Default network: " << default_network->name()
             << " State: " << default_connection_state;
    return true;
  }

  if (IsCaptivePortal(default_network)) {
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

  std::move(callback).Run();
}

}  // namespace ash
