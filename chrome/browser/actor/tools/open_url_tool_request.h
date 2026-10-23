// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_REQUEST_H_

#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace actor {

class ToolRequestVisitorFunctor;

// Opens `url` relative to the given actor surface:
//   - If `open_in_new_tab` is false, the surface itself is navigated to `url`.
//   - If `open_in_new_tab` is true, a new foreground tab is created in the
//     surface's window and navigated to `url`.
class OpenUrlToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "OpenUrl";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "open_url";
  // JSON argument key for the destination URL parameter.
  static constexpr std::string_view kUrlParam = "url";
  // JSON argument key for whether to open the URL in a new tab.
  static constexpr std::string_view kNewTabParam = "new_tab";

  OpenUrlToolRequest(ActorSurfaceHandle actor_surface_handle,
                     GURL url,
                     bool open_in_new_tab);
  ~OpenUrlToolRequest() override;
  OpenUrlToolRequest(const OpenUrlToolRequest&);
  OpenUrlToolRequest& operator=(const OpenUrlToolRequest&);

  // Returns the `ToolId::kOpenUrl` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  const GURL& url() const { return url_; }
  bool open_in_new_tab() const { return open_in_new_tab_; }

  // ToolRequest:
  bool RequiresUrlCheckInCurrentTab() const override;
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
  std::optional<url::Origin> AssociatedOriginGrant() const override;

 private:
  GURL url_;
  bool open_in_new_tab_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OPEN_URL_TOOL_REQUEST_H_
