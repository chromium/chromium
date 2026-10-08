// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Chooses an option in a <select> box on the page based on the value attribute
// of the <option> children.
class SelectToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "Select";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "select_option";
  // JSON argument key for the DOM node ID of the target <select> element.
  static constexpr std::string_view kDomNodeIdParam = "dom_node_id";
  // JSON argument key for the value attribute of the <option> to select.
  static constexpr std::string_view kValueParam = "value";

  SelectToolRequest(tabs::TabHandle tab_handle,
                    const PageTarget& target,
                    std::string_view value);
  ~SelectToolRequest() override;

  // Returns the `ToolId::kSelectOption` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  // The <option> whose value attribute matches this parameter will be selected.
  std::string value_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SELECT_TOOL_REQUEST_H_
