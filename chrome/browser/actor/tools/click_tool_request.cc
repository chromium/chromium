// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/click_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom-shared.h"
#include "content/public/browser/render_widget_host.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"

namespace actor {

namespace {

// Default description for the `click` tool.
constexpr std::string_view kClickToolDescription =
    "Click or toggle an interactive element (such as a button, radio button, "
    "or checkbox) on the active webpage.";

// Description for the `dom_node_id` parameter of the `click` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The numeric DOM node ID of the target element (e.g. 101).";

}  // namespace

using ::tabs::TabHandle;

ClickToolRequest::ClickToolRequest(
    TabHandle tab_handle,
    const PageTarget& target,
    mojom::ClickType type,
    mojom::ClickCount count,
    bool requires_opening_web_contents,
    std::optional<ObservationDelayController::PageStabilityConfig>
        page_stability_config)
    : PageToolRequest(tab_handle, target),
      click_type_(type),
      click_count_(count),
      requires_opening_web_contents_(requires_opening_web_contents),
      page_stability_config_(page_stability_config) {}

ClickToolRequest::~ClickToolRequest() = default;

// static
std::optional<ToolDefinition> ClickToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kClick, kModelFacingName,
                               kClickToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kDomNodeIdParam, kDomNodeIdParamDescription))
      .Build();
}

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
  if (page_stability_config_.has_value()) {
    return *page_stability_config_;
  }
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
