// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_MCP_SERVER_PREFS_H_
#define CHROME_BROWSER_MCP_SERVER_MCP_SERVER_PREFS_H_

namespace user_prefs {
class PrefRegistrySyncable;
}

namespace mcp_server {

// Preference keys for MCP Server settings
extern const char kMcpServerEnabled[];
extern const char kMcpServerPort[];

// Registers profile-level preferences for MCP Server
void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_MCP_SERVER_PREFS_H_
