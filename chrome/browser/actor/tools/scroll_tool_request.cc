// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/scroll_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "third_party/blink/public/mojom/input/scroll_direction.mojom-shared.h"
#include "ui/events/types/scroll_types.h"

namespace actor {

namespace {

// Default description for the `scroll` tool.
constexpr std::string_view kScrollToolDescription =
    "Scrolls the page or a specific scrollable element in the given direction.";

// Description for the `direction` parameter of the `scroll` tool.
constexpr std::string_view kDirectionParamDescription =
    "The direction to scroll.";

// Allowed values for the `direction` parameter of the `scroll` tool.
constexpr std::string_view kDirectionParamValues[] = {"up", "down", "left",
                                                      "right"};

// Description for the `distance` parameter of the `scroll` tool.
constexpr std::string_view kDistanceParamDescription =
    "Distance to scroll in pixels.";

// Description for the `dom_node_id` parameter of the `scroll` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The DOM node ID of the scrollable element, or 0 to scroll the main "
    "viewport.";

// Default description for the `scroll_page` tool.
constexpr std::string_view kScrollPageToolDescription =
    "Scrolls the page or a specific scrollable element in the given direction "
    "by one page, or all the way to its start or end.";

// Description for the `granularity` parameter of the `scroll_page` tool.
constexpr std::string_view kGranularityParamDescription =
    "The units to scroll by. \"page\" scrolls by one page of the scroller. "
    "\"document\" scrolls to the start or end of the scroller.";

// Allowed values for the `granularity` parameter of the `scroll_page` tool.
constexpr std::string_view kGranularityParamValues[] = {"page", "document"};

blink::mojom::ScrollDirectionPhysical ToMojoDirection(
    ScrollToolRequest::Direction direction) {
  switch (direction) {
    case ScrollToolRequest::Direction::kLeft:
      return blink::mojom::ScrollDirectionPhysical::kLeft;
    case ScrollToolRequest::Direction::kRight:
      return blink::mojom::ScrollDirectionPhysical::kRight;
    case ScrollToolRequest::Direction::kUp:
      return blink::mojom::ScrollDirectionPhysical::kUp;
    case ScrollToolRequest::Direction::kDown:
      return blink::mojom::ScrollDirectionPhysical::kDown;
  }
  NOTREACHED();
}

}  // namespace


ScrollToolRequest::ScrollToolRequest(ActorSurfaceHandle actor_surface_handle,
                                     const PageTarget& target,
                                     Direction direction,
                                     float distance)
    : PageToolRequest(actor_surface_handle, target),
      direction_(direction),
      distance_(distance) {}

ScrollToolRequest::~ScrollToolRequest() = default;

// static
std::optional<ToolDefinition> ScrollToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kScroll, kModelFacingName,
                               kScrollToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddStringEnumProperty(kDirectionParam,
                                     kDirectionParamDescription,
                                     kDirectionParamValues)
              .AddNumberProperty(kDistanceParam, kDistanceParamDescription)
              .AddIntegerProperty(kDomNodeIdParam, kDomNodeIdParamDescription))
      .Build();
}

void ScrollToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ScrollToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ScrollToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto scroll = mojom::ScrollAction::New();
  scroll->direction = ToMojoDirection(direction_);
  scroll->amount = mojom::ScrollAmount::NewDistance(distance_);
  return mojom::ToolAction::NewScroll(std::move(scroll));
}

std::unique_ptr<PageToolRequest> ScrollToolRequest::Clone() const {
  return std::make_unique<ScrollToolRequest>(*this);
}

ScrollPageToolRequest::ScrollPageToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& target,
    Granularity granularity,
    Direction direction)
    : PageToolRequest(actor_surface_handle, target),
      granularity_(granularity),
      direction_(direction) {}

ScrollPageToolRequest::ScrollPageToolRequest(
    const ScrollPageToolRequest& other) = default;

ScrollPageToolRequest::~ScrollPageToolRequest() = default;

// static
std::optional<ToolDefinition> ScrollPageToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kScrollPage, kModelFacingName,
                               kScrollPageToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddStringEnumProperty(kGranularityParam,
                                     kGranularityParamDescription,
                                     kGranularityParamValues)
              .AddStringEnumProperty(kDirectionParam,
                                     kDirectionParamDescription,
                                     kDirectionParamValues)
              .AddIntegerProperty(kDomNodeIdParam, kDomNodeIdParamDescription))
      .Build();
}

void ScrollPageToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ScrollPageToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ScrollPageToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto scroll = mojom::ScrollAction::New();
  scroll->direction = ToMojoDirection(direction_);
  switch (granularity_) {
    case Granularity::kPage:
      scroll->amount = mojom::ScrollAmount::NewGranularity(
          ::ui::ScrollGranularity::kScrollByPage);
      break;
    case Granularity::kDocument:
      scroll->amount = mojom::ScrollAmount::NewGranularity(
          ::ui::ScrollGranularity::kScrollByDocument);
      break;
  }
  return mojom::ToolAction::NewScroll(std::move(scroll));
}

std::unique_ptr<PageToolRequest> ScrollPageToolRequest::Clone() const {
  return std::make_unique<ScrollPageToolRequest>(*this);
}

}  // namespace actor
