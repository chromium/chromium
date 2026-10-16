// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

namespace {

// Default description for the `wait` tool.
constexpr std::string_view kWaitToolDescription =
    "Pauses execution for the specified duration in milliseconds.";

// Description for the `wait_time_ms` parameter of the `wait` tool.
constexpr std::string_view kWaitTimeMsParamDescription =
    "The duration to wait in milliseconds.";

}  // namespace

WaitToolRequest::WaitToolRequest(
    base::TimeDelta wait_duration,
    ActorSurfaceHandle observe_actor_surface_handle)
    : wait_duration_(wait_duration),
      observe_actor_surface_handle_(observe_actor_surface_handle) {}

WaitToolRequest::~WaitToolRequest() = default;

// static
std::optional<ToolDefinition> WaitToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kWait, kModelFacingName,
                               kWaitToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kWaitTimeMsParam, kWaitTimeMsParamDescription))
      .Build();
}

ToolRequest::CreateToolResult WaitToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WaitTool>(task_id, tool_delegate, wait_duration_,
                                     observe_actor_surface_handle_),
          MakeOkResult()};
}

void WaitToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view WaitToolRequest::Name() const {
  return kName;
}

bool WaitToolRequest::RequiresUrlCheckInCurrentTab() const {
  return !observe_actor_surface_handle_.is_null();
}

ActorSurfaceHandle WaitToolRequest::GetActorSurfaceForValidation() const {
  return observe_actor_surface_handle_;
}

}  // namespace actor
