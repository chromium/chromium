// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    tabs::TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest& AttemptOtpFillingToolRequest::operator=(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest::~AttemptOtpFillingToolRequest() = default;

ToolRequest::CreateToolResult AttemptOtpFillingToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<AttemptOtpFillingTool>(task_id, tool_delegate),
          MakeOkResult()};
}

std::string_view AttemptOtpFillingToolRequest::Name() const {
  return kName;
}

void AttemptOtpFillingToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

}  // namespace actor
