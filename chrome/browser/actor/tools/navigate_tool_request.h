// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/tool_request.h"
#include "url/gurl.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Navigates a specified tab to a specified URL.
class NavigateToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "Navigate";

  NavigateToolRequest(tabs::TabHandle tab_handle, GURL url);
  ~NavigateToolRequest() override;

  bool RequiresUrlCheckInCurrentTab() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;

  std::optional<url::Origin> AssociatedOriginGrant() const override;

 private:
  GURL url_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
