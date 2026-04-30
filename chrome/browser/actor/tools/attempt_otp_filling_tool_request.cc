// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_otp_filling_tool_request.h"

#include <memory>
#include <string_view>
#include <utility>
#include <vector>

#include "chrome/browser/actor/tools/attempt_otp_filling_tool.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"

namespace actor {

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    tabs::TabHandle tab_handle,
    std::vector<PageTarget> trigger_fields,
    bool for_signin)
    : TabToolRequest(tab_handle),
      trigger_fields_(std::move(trigger_fields)),
      for_signin_(for_signin) {}

AttemptOtpFillingToolRequest::AttemptOtpFillingToolRequest(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest& AttemptOtpFillingToolRequest::operator=(
    const AttemptOtpFillingToolRequest&) = default;

AttemptOtpFillingToolRequest::~AttemptOtpFillingToolRequest() = default;

ToolRequest::CreateToolResult AttemptOtpFillingToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {
      std::make_unique<AttemptOtpFillingTool>(
          task_id, tool_delegate, GetTabHandle(), trigger_fields_, for_signin_),
      MakeOkResult()};
}

std::string_view AttemptOtpFillingToolRequest::Name() const {
  return kName;
}

void AttemptOtpFillingToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

}  // namespace actor
