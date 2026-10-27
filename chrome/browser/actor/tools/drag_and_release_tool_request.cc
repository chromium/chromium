// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"

#include <optional>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace actor {

namespace {

// Default description for the `drag_and_release` tool.
constexpr std::string_view kDragAndReleaseToolDescription =
    "Drags an element and releases it onto another element on the active "
    "webpage.";

// Description for the `from_dom_node_id` parameter of the `drag_and_release`
// tool.
constexpr std::string_view kFromDomNodeIdParamDescription =
    "The numeric DOM node ID of the element to drag.";

// Description for the `to_dom_node_id` parameter of the `drag_and_release`
// tool.
constexpr std::string_view kToDomNodeIdParamDescription =
    "The numeric DOM node ID of the element to drop onto.";

}  // namespace


DragAndReleaseToolRequest::DragAndReleaseToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& from_target,
    const PageTarget& to_target)
    : PageToolRequest(actor_surface_handle, from_target),
      from_target_(from_target),
      to_target_(to_target) {}

DragAndReleaseToolRequest::~DragAndReleaseToolRequest() = default;

DragAndReleaseToolRequest::DragAndReleaseToolRequest(
    const DragAndReleaseToolRequest& other) = default;

// static
std::optional<ToolDefinition> DragAndReleaseToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kDragAndRelease, kModelFacingName,
                               kDragAndReleaseToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddIntegerProperty(kFromDomNodeIdParam,
                                  kFromDomNodeIdParamDescription)
              .AddIntegerProperty(kToDomNodeIdParam,
                                  kToDomNodeIdParamDescription))
      .Build();
}

void DragAndReleaseToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view DragAndReleaseToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr DragAndReleaseToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto drag = mojom::DragAndReleaseAction::New();

  if (std::holds_alternative<gfx::Point>(to_target_)) {
    const gfx::Point& to_point = std::get<gfx::Point>(to_target_);
    PageTarget transformed_to_target =
        gfx::ToRoundedPoint(frame.GetView()->TransformRootPointToViewCoordSpace(
            gfx::PointF(to_point)));
    drag->to_target = ToMojo(transformed_to_target);
  } else {
    drag->to_target = ToMojo(to_target_);
  }

  return mojom::ToolAction::NewDragAndRelease(std::move(drag));
}

std::unique_ptr<PageToolRequest> DragAndReleaseToolRequest::Clone() const {
  return std::make_unique<DragAndReleaseToolRequest>(*this);
}

}  // namespace actor
