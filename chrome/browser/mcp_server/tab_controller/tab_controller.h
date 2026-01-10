// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_
#define CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_

#include <string>
#include <vector>

namespace mcp_server {

// TabController manages browser tabs
// Provides APIs for creating, closing, activating, and querying tabs
class TabController {
 public:
  TabController();
  ~TabController();

  // List all tabs
  std::string ListTabs();

  // Create a new tab with optional URL
  std::string CreateTab(const std::string& url);

  // Close a tab by ID
  bool CloseTab(int tab_id);

  // Activate/focus a tab
  bool ActivateTab(int tab_id);

  // Get tab state (URL, title, loading status)
  std::string GetTabState(int tab_id);

 private:
  // TODO: Implement tab management
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_
