// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {
class ToolRequestVisitorFunctor;

// Scrolls an element or viewport in the page a given distance.
class ScrollToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "Scroll";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "scroll";
  // JSON argument key for the scroll direction parameter.
  static constexpr std::string_view kDirectionParam = "direction";
  // JSON argument key for the scroll distance parameter in pixels.
  static constexpr std::string_view kDistanceParam = "distance";
  // JSON argument key for the target element DOM node ID parameter.
  static constexpr std::string_view kDomNodeIdParam = "dom_node_id";

  enum class Direction { kLeft, kRight, kUp, kDown };

  // Programmatically scrolls the scroller specified by target a given distance.
  // If Target is a nullopt ContentNodeId, the root viewport is scrolled.
  // Distance is specified in DIPs.
  ScrollToolRequest(tabs::TabHandle tab_handle,
                    const PageTarget& target,
                    Direction direction,
                    float distance);
  ~ScrollToolRequest() override;

  // Returns the `ToolId::kScroll` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

 private:
  Direction direction_;
  float distance_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
