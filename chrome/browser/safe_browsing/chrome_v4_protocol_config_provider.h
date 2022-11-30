// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_V4_PROTOCOL_CONFIG_PROVIDER_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_V4_PROTOCOL_CONFIG_PROVIDER_H_

#include <string>

#include "components/safe_browsing/core/browser/db/v4_protocol_manager_util.h"

namespace safe_browsing {

// Returns the client_name to use for Safe Browsing requests.
std::string GetProtocolConfigClientName();

// Create the default v4 protocol config struct.
V4ProtocolConfig GetV4ProtocolConfig();

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_V4_PROTOCOL_CONFIG_PROVIDER_H_
