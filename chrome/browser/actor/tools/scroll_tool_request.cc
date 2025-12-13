// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/scroll_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ScrollToolRequest::ScrollToolRequest(TabHandle tab_handle,
                                     const PageTarget& target,
                                     Direction direction,
                                     float distance)
    : PageToolRequest(tab_handle, target),
      direction_(direction),
      distance_(distance) {}

ScrollToolRequest::~ScrollToolRequest() = default;

void ScrollToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ScrollToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ScrollToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto scroll = mojom::ScrollAction::New();

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
