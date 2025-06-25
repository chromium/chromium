// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/drag_and_release_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

DragAndReleaseToolRequest::DragAndReleaseToolRequest(TabHandle tab_handle,
                                                     const Target& from_target,
                                                     const Target& to_target)
    : PageToolRequest(tab_handle, from_target),
      from_target_(from_target),
      to_target_(to_target) {}

DragAndReleaseToolRequest::~DragAndReleaseToolRequest() = default;

std::string DragAndReleaseToolRequest::JournalEvent() const {
  return "DragAndRelease";
}

mojom::ToolActionPtr DragAndReleaseToolRequest::ToMojoToolAction() const {
  auto drag = mojom::DragAndReleaseAction::New();

  drag->from_target = PageToolRequest::ToMojoToolTarget(from_target_);
  drag->to_target = PageToolRequest::ToMojoToolTarget(to_target_);

  return mojom::ToolAction::NewDragAndRelease(std::move(drag));
}

std::unique_ptr<PageToolRequest> DragAndReleaseToolRequest::Clone() const {
  return std::make_unique<DragAndReleaseToolRequest>(*this);
}

}  // namespace actor
