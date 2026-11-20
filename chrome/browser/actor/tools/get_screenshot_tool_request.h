// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_REQUEST_H_

#include <optional>
#include <string_view>

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"

namespace content {
class Page;
}  // namespace content

namespace actor {

class ToolRequestVisitorFunctor;

// Captures a screenshot of the active viewport of a tab.
class GetScreenshotToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "GetScreenshot";
  static constexpr std::string_view kModelFacingName = "get_screenshot";

  explicit GetScreenshotToolRequest(ActorSurfaceHandle actor_surface_handle);
  GetScreenshotToolRequest(const GetScreenshotToolRequest&);
  GetScreenshotToolRequest& operator=(const GetScreenshotToolRequest&);
  ~GetScreenshotToolRequest() override;

  // Returns the `ToolId::kGetScreenshot` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  base::WeakPtr<content::Page> target_page_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_GET_SCREENSHOT_TOOL_REQUEST_H_
