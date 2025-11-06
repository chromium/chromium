// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

using ::tabs::TabHandle;
using ::tabs::TabInterface;

HistoryToolRequest::HistoryToolRequest(tabs::TabHandle tab, Direction direction)
    : TabToolRequest(tab), direction_(direction) {}
HistoryToolRequest::~HistoryToolRequest() = default;

ToolRequest::CreateToolResult HistoryToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetTabHandle().Get();

  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  CHECK(tab->GetContents());
  return {
      std::make_unique<HistoryTool>(task_id, tool_delegate, *tab, direction_),
      MakeOkResult()};
}

void HistoryToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view HistoryToolRequest::Name() const {
  return kName;
}

bool HistoryToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A history tool is tab scoped but navigates *away* from the current URL --
  // the destination URL is checked in HistoryTool::Validate().
  return false;
}

}  // namespace actor
