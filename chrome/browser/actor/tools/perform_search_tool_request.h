// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_PERFORM_SEARCH_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_PERFORM_SEARCH_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Navigates a specified tab to a search url constructed using a provided
// search query and the profile's default search engine.
class PerformSearchToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "PerformSearch";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "perform_search";
  // JSON argument key for the search query parameter.
  static constexpr std::string_view kQueryParam = "query";

  PerformSearchToolRequest(tabs::TabHandle tab_handle, std::string query);
  ~PerformSearchToolRequest() override;

  // Returns the `ToolId::kPerformSearch` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  bool RequiresUrlCheckInCurrentTab() const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  std::string_view Name() const override;

  const std::string& query() const { return query_; }

 private:
  std::string query_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_PERFORM_SEARCH_TOOL_REQUEST_H_
