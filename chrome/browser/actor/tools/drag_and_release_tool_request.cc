// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "drag_and_release_tool_request.h"
#include "ui/gfx/geometry/point_conversions.h"

namespace actor {

using ::tabs::TabHandle;

DragAndReleaseToolRequest::DragAndReleaseToolRequest(
    TabHandle tab_handle,
    const PageTarget& from_target,
    const PageTarget& to_target)
    : PageToolRequest(tab_handle, from_target),
      from_target_(from_target),
      to_target_(to_target) {}

DragAndReleaseToolRequest::~DragAndReleaseToolRequest() = default;

DragAndReleaseToolRequest::DragAndReleaseToolRequest(
    const DragAndReleaseToolRequest& other) = default;

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
