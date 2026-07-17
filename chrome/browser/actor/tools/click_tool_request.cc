// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom-shared.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace actor {

using ::tabs::TabHandle;

ClickToolRequest::ClickToolRequest(TabHandle tab_handle,
                                   const PageTarget& target,
                                   mojom::ClickType type,
                                   mojom::ClickCount count)
    : ClickToolRequest(tab_handle,
                       target,
                       type,
                       count,
                       /*requires_opening_web_contents=*/false) {}

ClickToolRequest::ClickToolRequest(TabHandle tab_handle,
                                   const PageTarget& target,
                                   mojom::ClickType type,
                                   mojom::ClickCount count,
                                   bool requires_opening_web_contents)
    : PageToolRequest(tab_handle, target),
      click_type_(type),
      click_count_(count),
      requires_opening_web_contents_(requires_opening_web_contents) {}

ClickToolRequest::~ClickToolRequest() = default;

void ClickToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ClickToolRequest::Name() const {
  return kName;
}

bool ClickToolRequest::RequiresOpeningWebContents() const {
  return requires_opening_web_contents_ ||
         PageToolRequest::RequiresOpeningWebContents();
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

bool ClickToolRequest::RequiresTargetInLastApc() const {
  // Direct activation bypasses hit testing, so its target must appear in the
  // last APC even when TOCTOU validation is off.
  return click_type_ == mojom::ClickType::kLeftOnOccludedTarget ||
         PageToolRequest::RequiresTargetInLastApc();
}

bool ClickToolRequest::IsSubframeTargetingAllowed() const {
  return click_type_ != mojom::ClickType::kLeftOnOccludedTarget;
}

ObservationDelayController::PageStabilityConfig
ClickToolRequest::GetObservationPageStabilityConfig() const {
  return ObservationDelayController::PageStabilityConfig{
      .supports_paint_stability = true,
  };
}

void ClickToolRequest::WillSendToRenderer(
    content::RenderWidgetHost* render_widget_host) {
  blink::WebMouseEvent event = blink::WebMouseEvent();
  event.SetType(blink::WebInputEvent::Type::kMouseDown);

  // Trigger user interaction notification.
  render_widget_host->SimulateUserInteraction(event);
}

}  // namespace actor
