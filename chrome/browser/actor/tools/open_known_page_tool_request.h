// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_REQUEST_H_

#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/task_id.h"

namespace actor {

class ToolDelegate;
class ToolRequestVisitorFunctor;

// Opens a known page matching a fuzzy `query` in the currently active browser
// window, searching in strict cascading order:
//   1. Open tabs in the currently active browser window (`FindMatchingTabs`)
//   2. Browsing history (`FindMatchingHistory`)
//   3. Bookmarks (`FindMatchingBookmarks`)
//
// Only the profile's most recently active window (resolved at `CreateTool()`
// time) is searched for open tabs or actuated on. When an open background tab
// matches, it is activated in that window; when a URL is resolved from history
// or bookmarks, the active tab in that window is navigated to the resolved URL.
//
// This derives from ToolRequest rather than TabToolRequest because the target
// tab is not known when the request is created and is resolved during
// OpenKnownPageTool::Validate().
class OpenKnownPageToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "OpenKnownPage";

  explicit OpenKnownPageToolRequest(std::string query);
  ~OpenKnownPageToolRequest() override;

  OpenKnownPageToolRequest(const OpenKnownPageToolRequest&);
  OpenKnownPageToolRequest& operator=(const OpenKnownPageToolRequest&);

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  std::string query_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_REQUEST_H_
