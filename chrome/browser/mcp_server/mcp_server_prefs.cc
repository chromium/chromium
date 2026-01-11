// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server_prefs.h"

#include "components/pref_registry/pref_registry_syncable.h"
#include "components/prefs/pref_registry_simple.h"

namespace mcp_server {

// Boolean pref indicating whether MCP Server is enabled
const char kMcpServerEnabled[] = "ai_features.mcp_server_enabled";

// Integer pref for MCP Server port number
const char kMcpServerPort[] = "ai_features.mcp_server_port";

void RegisterLocalStatePrefs(PrefRegistrySimple* registry) {
  // Default: MCP Server is disabled
  registry->RegisterBooleanPref(kMcpServerEnabled, false);

  // Default port: 9224
  registry->RegisterIntegerPref(kMcpServerPort, 9224);
}

void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry) {
  // No profile-level prefs needed - MCP Server uses local_state (browser-wide)
  // This function is kept for compatibility with browser_prefs.cc
}

}  // namespace mcp_server
