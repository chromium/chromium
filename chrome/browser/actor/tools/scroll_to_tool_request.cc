// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/scroll_to_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ScrollToToolRequest::ScrollToToolRequest(TabHandle tab_handle,
                                         const PageTarget& target)
    : PageToolRequest(tab_handle, target) {}

ScrollToToolRequest::~ScrollToToolRequest() = default;

void ScrollToToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ScrollToToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ScrollToToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto action = mojom::ScrollToAction::New();
  return mojom::ToolAction::NewScrollTo(std::move(action));
}

std::unique_ptr<PageToolRequest> ScrollToToolRequest::Clone() const {
  return std::make_unique<ScrollToToolRequest>(*this);
}

}  // namespace actor
