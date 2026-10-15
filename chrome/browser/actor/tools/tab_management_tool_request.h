// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "ui/base/window_open_disposition.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Creates a new blank tab in the specified window.
class CreateTabToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "CreateTab";
  static constexpr std::string_view kModelFacingName = "create_tab";

  CreateTabToolRequest(int32_t window_id, WindowOpenDisposition disposition);
  ~CreateTabToolRequest() override;

  // Returns the `ToolId::kCreateTab` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  bool AddsTabToObservationSet() const override;

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  std::string_view Name() const override;

 private:
  int32_t window_id_;
  WindowOpenDisposition disposition_;
};

// Brings the specified tab to the foreground.
class ActivateTabToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "ActivateTab";
  static constexpr std::string_view kModelFacingName = "activate_tab";
  static constexpr std::string_view kTabIdParam = "tab_id";

  explicit ActivateTabToolRequest(ActorSurfaceHandle actor_surface_handle);
  ~ActivateTabToolRequest() override;

  // Returns the `ToolId::kActivateTab` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
};

// Closes the specified tab to the foreground.
class CloseTabToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "CloseTab";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "close_current_tab";

  explicit CloseTabToolRequest(ActorSurfaceHandle actor_surface_handle);
  ~CloseTabToolRequest() override;

  // Returns the `ToolId::kCloseTab` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_REQUEST_H_
