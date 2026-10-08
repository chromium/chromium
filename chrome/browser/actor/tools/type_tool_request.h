// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

class TypeToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "Type";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "type";
  // JSON argument key for the DOM node ID of the target editable element.
  static constexpr std::string_view kDomNodeIdParam = "dom_node_id";
  // JSON argument key for the text to type.
  static constexpr std::string_view kTextParam = "text";
  // JSON argument key for whether to press Enter after typing.
  static constexpr std::string_view kFollowByEnterParam = "follow_by_enter";

  enum class Mode {
    // Replace all existing text in the editing context.
    kReplace,

    // Insert text before any existing text in the editing context.
    kPrepend,

    // Insert text after any existing text in the editing context.
    kAppend
  };

  TypeToolRequest(tabs::TabHandle tab_handle,
                  const PageTarget& target,
                  std::string_view text,
                  bool follow_by_enter,
                  Mode mode);
  ~TypeToolRequest() override;

  // Returns the `ToolId::kType` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;
  std::string GetTextContentSentToRenderer() const override;
  ObservationDelayController::PageStabilityConfig
  GetObservationPageStabilityConfig() const override;

  // Text to type.
  std::string text;

  // Whether to inject an enter/return key after typing.
  bool follow_by_enter;

  // Behavior with respect to existing text.
  Mode mode;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TYPE_TOOL_REQUEST_H_
