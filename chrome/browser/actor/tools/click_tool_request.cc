// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

using ::tabs::TabHandle;

ClickToolRequest::ClickToolRequest(TabHandle tab_handle,
                                   const PageTarget& target,
                                   MouseClickType type,
                                   MouseClickCount count)
    : PageToolRequest(tab_handle, target),
      click_type_(type),
      click_count_(count) {}

ClickToolRequest::~ClickToolRequest() = default;

void ClickToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ClickToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ClickToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto click = mojom::ClickAction::New();
  click->type = click_type_;
  click->count = click_count_;
  return mojom::ToolAction::NewClick(std::move(click));
}

std::unique_ptr<PageToolRequest> ClickToolRequest::Clone() const {
  return std::make_unique<ClickToolRequest>(*this);
}

ObservationDelayController::PageStabilityConfig
ClickToolRequest::GetObservationPageStabilityConfig() const {
  return ObservationDelayController::PageStabilityConfig{
      .supports_paint_stability = true,
  };
}

}  // namespace actor
