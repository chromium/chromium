// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/open_url_tool_request.h"

#include <string_view>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

OpenUrlToolRequest::OpenUrlToolRequest(ActorSurfaceHandle actor_surface_handle,
                                       GURL url,
                                       bool open_in_new_tab)
    : TabToolRequest(actor_surface_handle),
      url_(std::move(url)),
      open_in_new_tab_(open_in_new_tab) {}

OpenUrlToolRequest::~OpenUrlToolRequest() = default;

OpenUrlToolRequest::OpenUrlToolRequest(const OpenUrlToolRequest&) = default;
OpenUrlToolRequest& OpenUrlToolRequest::operator=(const OpenUrlToolRequest&) =
    default;

bool OpenUrlToolRequest::RequiresUrlCheckInCurrentTab() const {
  // This tool either navigates *away* from the current URL or opens a new tab;
  // it never acts within the current page.
  return false;
}

ToolRequest::CreateToolResult OpenUrlToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (!url_.is_valid()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "The URL is invalid.")};
  }

  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  // TODO(b/573158576): Create an OpenUrlTool once it is implemented.
  return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kToolUnknown,
                                       /*requires_page_stabilization=*/false,
                                       "OpenUrl is not implemented yet.")};
}

void OpenUrlToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view OpenUrlToolRequest::Name() const {
  return kName;
}

}  // namespace actor
