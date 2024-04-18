// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions to wait for network state.

#ifndef CHROME_BROWSER_ASH_NET_DELAY_NETWORK_CALL_H_
#define CHROME_BROWSER_ASH_NET_DELAY_NETWORK_CALL_H_

#include "base/functional/callback_forward.h"

namespace base {
class TimeDelta;
}

namespace ash {

// Returns `true` if network calls will be delayed by `DelayNetworkCall()`.
bool AreNetworkCallsDelayed();

// Delay callback until the network is connected or while on a captive portal.
// Also see `AreNetworkCallsDelayed()`.
void DelayNetworkCall(base::OnceClosure callback);

// Same as above `DelayNetworkCall()` except it allows a custom `retry_delay` to
// be passed.
void DelayNetworkCallWithCustomDelay(base::OnceClosure callback,
                                     base::TimeDelta retry_delay);

// Sets DelayNetworkCallsForTesting for compatibility with the deprecated
// NetworkPortalDetector::CaptivePortalStatus which defaulted to kUnknown
// causing IsCaptivePortal to return true and delaying network calls.
// See b/333450354 for details.
void SetDelayNetworkCallsForTesting(bool delay_network_calls);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_DELAY_NETWORK_CALL_H_
