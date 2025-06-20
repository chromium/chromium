// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/move_mouse_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

MoveMouseToolRequest::MoveMouseToolRequest(TabHandle tab_handle,
                                           const Target& target)
    : PageToolRequest(tab_handle, target) {}

MoveMouseToolRequest::~MoveMouseToolRequest() = default;

std::string MoveMouseToolRequest::JournalEvent() const {
  return "MoveMouse";
}

mojom::ToolActionPtr MoveMouseToolRequest::ToMojoToolAction() const {
  auto move_mouse = mojom::MouseMoveAction::New();

  move_mouse->target = PageToolRequest::ToMojoToolTarget(GetTarget());

  return mojom::ToolAction::NewMouseMove(std::move(move_mouse));
}

std::unique_ptr<PageToolRequest> MoveMouseToolRequest::Clone() const {
  return std::make_unique<MoveMouseToolRequest>(*this);
}

}  // namespace actor
