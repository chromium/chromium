// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_
#define CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_

#include <string>
#include <vector>

#include "base/values.h"

namespace content {
class WebContents;
}

namespace mcp_server {

// TabController manages browser tabs
// Provides APIs for creating, closing, activating, and querying tabs
class TabController {
 public:
  TabController();
  ~TabController();

  // List all tabs across all browser windows
  // Returns JSON array with tab information
  std::string ListTabs();

  // Create a new tab with specified URL
  // Returns JSON object with created tab info
  std::string CreateTab(const std::string& url);

  // Close a tab by session ID
  // Returns true if tab was found and closed
  bool CloseTab(int session_id);

  // Activate/focus a tab by session ID
  // Returns true if tab was found and activated
  bool ActivateTab(int session_id);

  // Get tab state (URL, title, loading status) by session ID
  // Returns JSON object with tab state
  std::string GetTabState(int session_id);

 private:
  // Find WebContents by session ID
  // Returns nullptr if not found
  content::WebContents* FindWebContentsBySessionId(int session_id);

  // Build JSON object for a single tab
  base::Value::Dict BuildTabInfo(content::WebContents* web_contents);
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_TAB_CONTROLLER_TAB_CONTROLLER_H_
