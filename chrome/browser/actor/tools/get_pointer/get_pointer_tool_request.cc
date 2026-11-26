// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_pointer/get_pointer_tool_request.h"

#include <optional>
#include <string_view>
#include <utility>

#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

namespace {

// The part of the description that does not depend on a request's options.
// TODO(crbug.com/587484155): `ttc::GetPointerTool::GetDefinition()` has a
// copy of this and adds what depends on the options. Make it use this one.
constexpr char kGetPointerToolDescription[] =
    "Returns a small image of the active webpage around where the user's "
    "pointer has been during their current request, with a crosshair marking "
    "the pointed-at spot, plus the page element under it. Use it when the "
    "user refers to something on screen with words like \"this\", \"that\", "
    "\"here\" or \"there\" without naming it. If it returns "
    "no_active_pointer, the user is not pointing at anything; answer from "
    "the page content or ask.";

}  // namespace

GetPointerToolRequest::GetPointerToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    std::vector<PointerLocation> locations,
    GetPointerResultOptions options)
    : TabToolRequest(actor_surface_handle),
      locations_(std::move(locations)),
      options_(options) {}

GetPointerToolRequest::GetPointerToolRequest(const GetPointerToolRequest&) =
    default;
GetPointerToolRequest& GetPointerToolRequest::operator=(
    const GetPointerToolRequest&) = default;
GetPointerToolRequest::~GetPointerToolRequest() = default;

// static
std::optional<ToolDefinition> GetPointerToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kGetPointer, kModelFacingName,
                               kGetPointerToolDescription)
      .Build();
}

ToolRequest::CreateToolResult GetPointerToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  // TODO(crbug.com/587484155): Create the tool, which is added in a future CL.
  NOTIMPLEMENTED();
  return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kToolUnknown)};
}

void GetPointerToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view GetPointerToolRequest::Name() const {
  return kName;
}

}  // namespace actor
