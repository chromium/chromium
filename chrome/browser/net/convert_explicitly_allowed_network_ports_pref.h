// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_CONVERT_EXPLICITLY_ALLOWED_NETWORK_PORTS_PREF_H_
#define CHROME_BROWSER_NET_CONVERT_EXPLICITLY_ALLOWED_NETWORK_PORTS_PREF_H_

#include <stdint.h>

#include <vector>

class PrefService;

// Reads the preference kExplicitlyAllowedNetworkPorts and
// converts it to a vector of 16-bit unsigned integers. Ignores anything that
// is outside the 16-bit port range.
std::vector<uint16_t> ConvertExplicitlyAllowedNetworkPortsPref(
    PrefService* local_state);

#endif  // CHROME_BROWSER_NET_CONVERT_EXPLICITLY_ALLOWED_NETWORK_PORTS_PREF_H_
