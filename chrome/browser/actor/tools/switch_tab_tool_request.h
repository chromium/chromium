// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_REQUEST_H_

#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/tool_request.h"
#include "components/actor/core/task_id.h"

namespace actor {

class ToolDelegate;
class ToolRequestVisitorFunctor;

// Brings an already-open background tab in the currently active browser window
// to the foreground, identified by a fuzzy `query` matched against tab titles
// and URLs rather than by handle.
//
// Only tabs in the profile's most recently active window (resolved at
// `CreateTool()` time) are searched or activated. The currently active tab in
// that window is ignored during matching so that switching is never a no-op and
// does not trigger false ambiguity when the active tab and a single background
// tab share a keyword.
//
// This derives from ToolRequest rather than TabToolRequest on purpose: the
// target tab is not known when the request is created. It is resolved by
// SwitchTabTool::Validate(), which fails the action if the query matches no
// background tab or more than one.
class SwitchTabToolRequest : public ToolRequest {
 public:
  static constexpr char kName[] = "SwitchTab";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "switch_tab";
  // JSON argument key for the search query string parameter.
  static constexpr std::string_view kQueryParam = "query";

  explicit SwitchTabToolRequest(std::string query);
  ~SwitchTabToolRequest() override;

  SwitchTabToolRequest(const SwitchTabToolRequest&);
  SwitchTabToolRequest& operator=(const SwitchTabToolRequest&);

  // Returns the `ToolId::kSwitchTab` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  // ToolRequest:
  CreateToolResult CreateTool(TaskId task_id,
                              ToolDelegate& tool_delegate) const override;
  void Apply(ToolRequestVisitorFunctor& f) const override;
  std::string_view Name() const override;

 private:
  std::string query_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_REQUEST_H_
