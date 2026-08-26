// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/perform_search_tool_request.h"

#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include "base/notimplemented.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

using ::tabs::TabHandle;

PerformSearchToolRequest::PerformSearchToolRequest(TabHandle tab_handle,
                                                   std::string query)
    : TabToolRequest(tab_handle), query_(std::move(query)) {}

PerformSearchToolRequest::~PerformSearchToolRequest() = default;

// Uses the navigate tool, which is tab scoped, but navigates away from the
// current URL.
bool PerformSearchToolRequest::RequiresUrlCheckInCurrentTab() const {
  return false;
}

void PerformSearchToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult PerformSearchToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Search query is empty.")};
  }

  NOTIMPLEMENTED();
  return {/*tool=*/nullptr, MakeOkResult()};
}

std::string_view PerformSearchToolRequest::Name() const {
  return kName;
}

}  // namespace actor
