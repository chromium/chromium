// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/move_mouse_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

MoveMouseToolRequest::MoveMouseToolRequest(TabHandle tab_handle,
                                           const PageTarget& target)
    : PageToolRequest(tab_handle, target) {}

MoveMouseToolRequest::~MoveMouseToolRequest() = default;

void MoveMouseToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view MoveMouseToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr MoveMouseToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto move_mouse = mojom::MouseMoveAction::New();
  return mojom::ToolAction::NewMouseMove(std::move(move_mouse));
}

std::unique_ptr<PageToolRequest> MoveMouseToolRequest::Clone() const {
  return std::make_unique<MoveMouseToolRequest>(*this);
}

}  // namespace actor
