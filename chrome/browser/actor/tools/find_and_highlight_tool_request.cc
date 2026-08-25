// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"

#include <utility>

#include "chrome/browser/actor/tools/find_and_highlight_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

FindAndHighlightToolRequest::FindAndHighlightToolRequest(
    tabs::TabHandle tab_handle,
    std::string query)
    : TabToolRequest(tab_handle), query_(std::move(query)) {}

FindAndHighlightToolRequest::~FindAndHighlightToolRequest() = default;

FindAndHighlightToolRequest::FindAndHighlightToolRequest(
    const FindAndHighlightToolRequest&) = default;
FindAndHighlightToolRequest& FindAndHighlightToolRequest::operator=(
    const FindAndHighlightToolRequest&) = default;

ToolRequest::CreateToolResult FindAndHighlightToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Query cannot be empty.")};
  }

  if (!GetTabHandle().Get()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<FindAndHighlightTool>(task_id, tool_delegate,
                                                 GetTabHandle(), query_),
          MakeOkResult()};
}

void FindAndHighlightToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view FindAndHighlightToolRequest::Name() const {
  return kName;
}

}  // namespace actor
