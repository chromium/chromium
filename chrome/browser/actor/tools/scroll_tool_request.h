// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_

#include <memory>
#include <optional>
#include <string>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
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
  ScrollToolRequest(ActorSurfaceHandle actor_surface_handle,
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

// Scrolls an element or viewport in the page by a page, or to its start/end,
// the way the PageUp/PageDown/Home/End keys would. This is executed by the same
// renderer tool as ScrollToolRequest, but the scroll amount is determined in
// the renderer from the scroller's size, and the scroll may animate if smooth
// scrolling is enabled. Use ScrollToolRequest to scroll by a specific distance.
class ScrollPageToolRequest : public PageToolRequest {
 public:
  static constexpr char kName[] = "ScrollPage";
  // Canonical model-facing name exposed in LLM prompt schemas and indexed by
  // `ToolRegistry` for lookup and routing via `GetToolDefinition()`.
  static constexpr std::string_view kModelFacingName = "scroll_page";
  // JSON argument key for the scroll granularity parameter.
  static constexpr std::string_view kGranularityParam = "granularity";
  // JSON argument key for the scroll direction parameter.
  static constexpr std::string_view kDirectionParam = "direction";
  // JSON argument key for the target element DOM node ID parameter.
  static constexpr std::string_view kDomNodeIdParam = "dom_node_id";

  using Direction = ScrollToolRequest::Direction;

  enum class Granularity {
    // Scroll by a page of the scroller, like PageUp/PageDown.
    kPage,
    // Scroll to the start/end of the scroller, like Home/End.
    kDocument,
  };

  // Scrolls the scroller specified by `target`. A DomNode target with
  // kRootElementDomNodeId scrolls the viewport of that node's document.
  ScrollPageToolRequest(ActorSurfaceHandle actor_surface_handle,
                        const PageTarget& target,
                        Granularity granularity,
                        Direction direction);
  ScrollPageToolRequest(const ScrollPageToolRequest& other);
  ~ScrollPageToolRequest() override;

  // Returns the `ToolId::kScrollPage` tool schema definition.
  static std::optional<ToolDefinition> GetToolDefinition();

  void Apply(ToolRequestVisitorFunctor& f) const override;

  // ToolRequest
  std::string_view Name() const override;

  // PageToolRequest
  mojom::ToolActionPtr ToMojoToolAction(
      content::RenderFrameHost& frame) const override;
  std::unique_ptr<PageToolRequest> Clone() const override;

  Granularity granularity() const { return granularity_; }
  Direction direction() const { return direction_; }

 private:
  Granularity granularity_;
  Direction direction_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SCROLL_TOOL_REQUEST_H_
