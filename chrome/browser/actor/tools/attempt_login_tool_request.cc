// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool_request.h"

#include <optional>

#include "chrome/browser/actor/tools/attempt_login_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

AttemptLoginToolRequest::AttemptLoginToolRequest(tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

AttemptLoginToolRequest::~AttemptLoginToolRequest() = default;

ToolRequest::CreateToolResult AttemptLoginToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  tabs::TabInterface* tab = GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<AttemptLoginTool>(task_id, tool_delegate, *tab),
          MakeOkResult()};
}

void AttemptLoginToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view AttemptLoginToolRequest::Name() const {
  return kName;
}

}  // namespace actor
