// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/scroll_tool_request.h"

#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ScrollToolRequest::ScrollToolRequest(TabHandle tab_handle,
                                     const Target& target,
                                     Direction direction,
                                     float distance)
    : PageToolRequest(tab_handle, target),
      direction_(direction),
      distance_(distance) {}

ScrollToolRequest::~ScrollToolRequest() = default;

std::string ScrollToolRequest::JournalEvent() const {
  return "Scroll";
}

mojom::ToolActionPtr ScrollToolRequest::ToMojoToolAction() const {
  auto scroll = mojom::ScrollAction::New();

  scroll->target = PageToolRequest::ToMojoToolTarget(GetTarget());
  switch (direction_) {
    case Direction::kLeft:
      scroll->direction = mojom::ScrollAction::ScrollDirection::kLeft;
      break;
    case Direction::kRight:
      scroll->direction = mojom::ScrollAction::ScrollDirection::kRight;
      break;
    case Direction::kUp:
      scroll->direction = mojom::ScrollAction::ScrollDirection::kUp;
      break;
    case Direction::kDown:
      scroll->direction = mojom::ScrollAction::ScrollDirection::kDown;
      break;
  }

  scroll->distance = distance_;

  return mojom::ToolAction::NewScroll(std::move(scroll));
}

std::unique_ptr<PageToolRequest> ScrollToolRequest::Clone() const {
  return std::make_unique<ScrollToolRequest>(*this);
}

}  // namespace actor
