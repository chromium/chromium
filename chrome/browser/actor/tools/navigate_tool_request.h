// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "url/gurl.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Navigates a specified tab to a specified URL. If `open_in_new_tab` is true,
// instead opens a new foreground tab in the profile's most recently active
// normal window, adds it to the task, and navigates it to the URL. In that case
// `actor_surface_handle` must be null.
class NavigateToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "Navigate";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "navigate";
  // JSON argument key for the destination URL parameter.
  static constexpr std::string_view kUrlParam = "url";

  NavigateToolRequest(ActorSurfaceHandle actor_surface_handle,
                      GURL url,
                      bool open_in_new_tab = false);
  ~NavigateToolRequest() override;

  // Returns the `ToolId::kNavigate` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  const GURL& url() const { return url_; }
  bool open_in_new_tab() const { return open_in_new_tab_; }

  bool RequiresUrlCheckInCurrentTab() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool ActsOnWebContents() const override;
  ActorSurfaceHandle GetActorSurfaceHandle() const override;
  GURL GetURLForJournal() const override;

  std::optional<url::Origin> AssociatedOriginGrant() const override;

 private:
  // Null if and only if `open_in_new_tab_` is true. Otherwise, it may point to
  // an actor surface that no longer exists.
  ActorSurfaceHandle actor_surface_handle_;
  GURL url_;
  bool open_in_new_tab_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_NAVIGATE_TOOL_REQUEST_H_
