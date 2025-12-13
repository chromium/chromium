// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

using ::tabs::TabHandle;
using ::tabs::TabInterface;

NavigateToolRequest::NavigateToolRequest(TabHandle tab_handle, GURL url)
    : TabToolRequest(tab_handle), url_(url) {}

NavigateToolRequest::~NavigateToolRequest() = default;

ToolRequest::CreateToolResult NavigateToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<NavigateTool>(task_id, tool_delegate, *tab, url_),
          MakeOkResult()};
}

bool NavigateToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A navigate tool is tab scoped but navigates *away* from the current URL.
  return false;
}

void NavigateToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view NavigateToolRequest::Name() const {
  return kName;
}

std::optional<url::Origin> NavigateToolRequest::AssociatedOriginGrant() const {
  return url::Origin::Create(url_);
}

}  // namespace actor
