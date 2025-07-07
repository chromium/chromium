// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "drag_and_release_tool_request.h"

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

std::string DragAndReleaseToolRequest::JournalEvent() const {
  return "DragAndRelease";
}

mojom::ToolActionPtr DragAndReleaseToolRequest::ToMojoToolAction() const {
  auto drag = mojom::DragAndReleaseAction::New();

  drag->to_target = ToMojo(to_target_);

  return mojom::ToolAction::NewDragAndRelease(std::move(drag));
}

std::unique_ptr<PageToolRequest> DragAndReleaseToolRequest::Clone() const {
  return std::make_unique<DragAndReleaseToolRequest>(*this);
}

}  // namespace actor
