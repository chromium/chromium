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

// Default delay to be used as an argument to DelayNetworkCall().
extern const unsigned kDefaultNetworkRetryDelayMS;

// Delay callback until the network is connected or while on a captive portal.
void DelayNetworkCall(base::TimeDelta retry, base::OnceClosure callback);

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_NET_DELAY_NETWORK_CALL_H_
