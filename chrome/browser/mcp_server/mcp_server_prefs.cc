// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"

namespace mcp_server {

// Boolean pref indicating whether MCP Server is enabled
const char kMcpServerEnabled[] = "mcp_server.enabled";

// Integer pref for MCP Server port number
const char kMcpServerPort[] = "mcp_server.port";

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // Default: MCP Server is disabled
  registry->RegisterBooleanPref(kMcpServerEnabled, false);

  // Default port: 9224
  registry->RegisterIntegerPref(kMcpServerPort, 9224);
}

}  // namespace mcp_server
