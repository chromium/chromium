// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_request.h"

#include <optional>
#include <string_view>

#include "base/feature_list.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/script_tool_host.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"
#include "components/actor/core/actor_features.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "third_party/blink/public/common/features_generated.h"

namespace actor {

namespace {

// Default description for the `script` tool.
constexpr std::string_view kScriptToolDescription =
    "Executes a named script tool in the target document.";

// Description for the `document_identifier` parameter of the `script` tool.
constexpr std::string_view kDocumentIdentifierParamDescription =
    "The identifier of the document that exposes the script tool.";

// Description for the `tool_name` parameter of the `script` tool.
constexpr std::string_view kToolNameParamDescription =
    "The name of the script tool to execute.";

// Description for the `input_arguments` parameter of the `script` tool.
constexpr std::string_view kInputArgumentsParamDescription =
    "The input arguments string passed to the script tool.";

}  // namespace


ScriptToolRequest::ScriptToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const base::UnguessableToken& target_document_id,
    const std::string& name,
    const std::string& input_arguments)
    : TabToolRequest(actor_surface_handle),
      target_document_id_(target_document_id),
      name_(name),
      input_arguments_(input_arguments) {}

ScriptToolRequest::~ScriptToolRequest() = default;

// static
std::optional<ToolDefinition> ScriptToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kScript, kModelFacingName,
                               kScriptToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddStringProperty(kDocumentIdentifierParam,
                                 kDocumentIdentifierParamDescription)
              .AddStringProperty(kToolNameParam, kToolNameParamDescription)
              .AddStringProperty(kInputArgumentsParam,
                                 kInputArgumentsParamDescription))
      .Build();
}

std::string_view ScriptToolRequest::Name() const {
  return kName;
}

void ScriptToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult ScriptToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (!base::FeatureList::IsEnabled(blink::features::kWebMCP)) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kScriptToolInvocationFailed,
                       /*requires_page_stabilization=*/false,
                       "WebMCP is not enabled.")};
  }

  ActorSurfaceHandle actor_surface_handle = GetActorSurfaceHandle();
  if (!actor_surface_handle.Get()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  auto script = mojom::ScriptToolAction::New(name_, input_arguments_);
  return {std::make_unique<ScriptToolHost>(
              task_id, tool_delegate, actor_surface_handle, target_document_id_,
              mojom::ToolAction::NewScriptTool(std::move(script))),
          MakeOkResult()};
}

ObservationDelayController::PageStabilityConfig
ScriptToolRequest::GetObservationPageStabilityConfig() const {
  if (base::FeatureList::IsEnabled(kActorScriptToolDelayObservation)) {
    return {
        .supports_paint_stability = false,
        .start_delay =
            base::Milliseconds(kActorScriptToolDelayObservationMillis.Get()),
    };
  } else {
    return ToolRequest::GetObservationPageStabilityConfig();
  }
}

bool ScriptToolRequest::RequiresOpeningWebContents() const {
  // The functionality provided by a script tool may require opening new
  // windows. For example, login or payment flows must be performed in a
  // seperate top-level window.
  return true;
}
}  // namespace actor
