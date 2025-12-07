// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

WaitToolRequest::WaitToolRequest(base::TimeDelta wait_duration,
                                 tabs::TabHandle observe_tab_handle)
    : wait_duration_(wait_duration), observe_tab_handle_(observe_tab_handle) {}

WaitToolRequest::~WaitToolRequest() = default;

ToolRequest::CreateToolResult WaitToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WaitTool>(task_id, tool_delegate, wait_duration_,
                                     observe_tab_handle_),
          MakeOkResult()};
}

void WaitToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view WaitToolRequest::Name() const {
  return kName;
}

}  // namespace actor
