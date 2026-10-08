// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_

#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Invokes a history back traversal in a specified tab.
class HistoryBackToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "HistoryBack";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "go_back";

  explicit HistoryBackToolRequest(tabs::TabHandle tab_handle);
  ~HistoryBackToolRequest() override;

  // Returns the `ToolId::kGoBack` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool RequiresUrlCheckInCurrentTab() const override;
};

// Invokes a history forward traversal in a specified tab.
class HistoryForwardToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "HistoryForward";

  explicit HistoryForwardToolRequest(tabs::TabHandle tab_handle);
  ~HistoryForwardToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool RequiresUrlCheckInCurrentTab() const override;
};

// Invokes a page reload in a specified tab.
class ReloadPageToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "ReloadPage";

  explicit ReloadPageToolRequest(tabs::TabHandle tab_handle,
                                 bool bypass_cache = false);
  ~ReloadPageToolRequest() override;

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;
  bool RequiresUrlCheckInCurrentTab() const override;

 private:
  bool bypass_cache_ = false;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_HISTORY_TOOL_REQUEST_H_
