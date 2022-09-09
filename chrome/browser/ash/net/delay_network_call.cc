// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/net/delay_network_call.h"

#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "chromeos/ash/components/network/network_handler.h"
#include "chromeos/ash/components/network/network_state.h"
#include "chromeos/ash/components/network/network_state_handler.h"
#include "chromeos/ash/components/network/portal_detector/network_portal_detector.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "third_party/cros_system_api/dbus/service_constants.h"

namespace ash {

const unsigned kDefaultNetworkRetryDelayMS = 3000;

void DelayNetworkCall(base::TimeDelta retry, base::OnceClosure callback) {
  bool delay_network_call = false;
  const NetworkState* default_network =
      NetworkHandler::Get()->network_state_handler()->DefaultNetwork();
  if (!default_network) {
    delay_network_call = true;
    DVLOG(1) << "DelayNetworkCall: No default network.";
  } else {
    std:: string default_connection_state = default_network->connection_state();
    if (!NetworkState::StateIsConnected(default_connection_state)) {
      delay_network_call = true;
      DVLOG(1) << "DelayNetworkCall: "
               << "Default network: " << default_network->name()
               << " State: " << default_connection_state;
    }
  }
  if (!delay_network_call && network_portal_detector::IsInitialized()) {
    NetworkPortalDetector::CaptivePortalStatus status =
        network_portal_detector::GetInstance()->GetCaptivePortalStatus();
    if (status != NetworkPortalDetector::CAPTIVE_PORTAL_STATUS_ONLINE) {
      delay_network_call = true;
      DVLOG(1) << "DelayNetworkCall: Captive portal status for "
              << default_network->name() << ": "
              << NetworkPortalDetector::CaptivePortalStatusString(status);
    }
  }
  if (delay_network_call) {
    content::GetUIThreadTaskRunner({})->PostDelayedTask(
        FROM_HERE,
        base::BindOnce(&DelayNetworkCall, retry, std::move(callback)), retry);
  } else {
    std::move(callback).Run();
  }
}

}  // namespace ash
