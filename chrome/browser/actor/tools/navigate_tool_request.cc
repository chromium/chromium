// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool_request.h"

#include "chrome/browser/actor/tools/navigate_tool.h"
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
    AggregatedJournal& journal) const {
  TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<NavigateTool>(task_id, journal, *tab->GetContents(),
                                         url_),
          MakeOkResult()};
}

std::string NavigateToolRequest::JournalEvent() const {
  return "Navigate";
}

}  // namespace actor
