// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/actor/tools/wait_tool.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

WaitToolRequest::WaitToolRequest(base::TimeDelta wait_duration)
    : wait_duration_(wait_duration) {}

WaitToolRequest::~WaitToolRequest() = default;

ToolRequest::CreateToolResult WaitToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<WaitTool>(task_id, tool_delegate, wait_duration_),
          MakeOkResult()};
}

void WaitToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string WaitToolRequest::JournalEvent() const {
  return "Wait";
}

}  // namespace actor
