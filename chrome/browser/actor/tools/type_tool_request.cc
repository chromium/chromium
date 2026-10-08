// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/type_tool_request.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "components/actor/core/actor_features.h"

namespace actor {

namespace {

// Default description for the `type` tool.
constexpr std::string_view kTypeToolDescription =
    "Replaces the entire text content of an input field, textarea, search "
    "box, or editable element on the active webpage. Use this whenever the "
    "user asks to write, type, fill in, or enter text into a field or form on "
    "the page.";

// Description for the `dom_node_id` parameter of the `type` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The numeric DOM node ID of the target input field or editable element "
    "(e.g. 101).";

// Description for the `text` parameter of the `type` tool.
constexpr std::string_view kTextParamDescription =
    "The complete replacement text to set into the element.";

// Description for the `follow_by_enter` parameter of the `type` tool.
constexpr std::string_view kFollowByEnterParamDescription =
    "Whether to press Enter after typing the text (e.g. to submit a search).";

}  // namespace

using ::tabs::TabHandle;

TypeToolRequest::TypeToolRequest(TabHandle tab_handle,
                                 const PageTarget& target,
                                 std::string_view text,
                                 bool follow_by_enter,
                                 Mode mode)
    : PageToolRequest(tab_handle, target),
      text(text),
      follow_by_enter(follow_by_enter),
      mode(mode) {}

TypeToolRequest::~TypeToolRequest() = default;

// static
std::optional<ToolDefinition> TypeToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kType, kModelFacingName,
                               kTypeToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddIntegerProperty(kDomNodeIdParam, kDomNodeIdParamDescription)
              .AddStringProperty(kTextParam, kTextParamDescription)
              .AddBooleanProperty(kFollowByEnterParam,
                                  kFollowByEnterParamDescription))
      .Build();
}

void TypeToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view TypeToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr TypeToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto type = mojom::TypeAction::New();

  type->text = text;
  type->follow_by_enter = follow_by_enter;

  switch (mode) {
    case Mode::kReplace:
      type->mode = mojom::TypeAction::Mode::kDeleteExisting;
      break;
    case Mode::kPrepend:
      type->mode = mojom::TypeAction::Mode::kPrepend;
      break;
    case Mode::kAppend:
      type->mode = mojom::TypeAction::Mode::kAppend;
      break;
  }

  return mojom::ToolAction::NewType(std::move(type));
}

std::unique_ptr<PageToolRequest> TypeToolRequest::Clone() const {
  return std::make_unique<TypeToolRequest>(*this);
}

std::string TypeToolRequest::GetTextContentSentToRenderer() const {
  return text;
}

ObservationDelayController::PageStabilityConfig
TypeToolRequest::GetObservationPageStabilityConfig() const {
  ObservationDelayController::PageStabilityConfig config{
      .supports_paint_stability = true,
  };

  // Typing into input fields often causes custom made dropdowns to appear and
  // update content. These are often updated via async tasks that try to detect
  // when a user has finished typing. Delay observation to try to ensure the
  // page stability monitor kicks in only after these tasks have invoked.
  if (base::FeatureList::IsEnabled(kActorTypeToolObservationStartDelay)) {
    config.start_delay = kActorTypeToolObservationStartDelayDuration.Get();
  }
  return config;
}

}  // namespace actor
