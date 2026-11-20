// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/get_screenshot_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/get_screenshot_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "content/public/browser/page.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

constexpr std::string_view kGetScreenshotToolDescription =
    "Capture a screenshot of the active webpage's current viewport.";

}  // namespace

GetScreenshotToolRequest::GetScreenshotToolRequest(
    ActorSurfaceHandle actor_surface_handle)
    : TabToolRequest(actor_surface_handle) {
  if (ActorSurface* actor_surface = actor_surface_handle.Get();
      actor_surface && actor_surface->GetWebContents()) {
    target_page_ =
        actor_surface->GetWebContents()->GetPrimaryPage().GetWeakPtr();
  }
}

GetScreenshotToolRequest::GetScreenshotToolRequest(
    const GetScreenshotToolRequest&) = default;
GetScreenshotToolRequest& GetScreenshotToolRequest::operator=(
    const GetScreenshotToolRequest&) = default;
GetScreenshotToolRequest::~GetScreenshotToolRequest() = default;

// static
std::optional<ToolDefinition> GetScreenshotToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kGetScreenshot, kModelFacingName,
                               kGetScreenshotToolDescription)
      .Build();
}

ToolRequest::CreateToolResult GetScreenshotToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kTabWentAway)};
  }

  return {std::make_unique<GetScreenshotTool>(task_id, tool_delegate,
                                              *actor_surface, target_page_),
          MakeOkResult()};
}

void GetScreenshotToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view GetScreenshotToolRequest::Name() const {
  return kName;
}

}  // namespace actor
