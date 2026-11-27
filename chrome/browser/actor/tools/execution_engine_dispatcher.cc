// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execution_engine_dispatcher.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/extend.h"
#include "chrome/browser/actor/javascript_dialog_interceptor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

namespace {

// Merges a follow-up or subsequent stage `ActionResult` into the existing
// result for the top-level action, preserving parent message and metadata not
// set by the follow-up.
void MergeActionResult(mojom::ActionResult& target,
                       mojom::ActionResultPtr followup) {
  target.code = followup->code;
  target.requires_page_stabilization = target.requires_page_stabilization ||
                                       followup->requires_page_stabilization;
  // TODO(crbug.com/581884304): if both parent and follow-up have messages,
  // avoid overwriting.
  if (!IsOk(*followup) || !followup->message.empty()) {
    target.message = std::move(followup->message);
  }
  if (followup->script_tool_response) {
    target.script_tool_response = std::move(followup->script_tool_response);
  }
  if (followup->execution_end_time) {
    target.execution_end_time = followup->execution_end_time;
  }
  target.screenshot_policy =
      std::max(target.screenshot_policy, followup->screenshot_policy);
  target.page_content_policy =
      std::max(target.page_content_policy, followup->page_content_policy);
  if (followup->attempt_login_status.has_value()) {
    target.attempt_login_status = followup->attempt_login_status;
  }
}

void MergeInterceptedDialogs(
    std::vector<InterceptedJavaScriptDialog>& target,
    std::vector<InterceptedJavaScriptDialog> followup) {
  if (target.size() >= JavaScriptDialogInterceptor::kMaxInterceptedDialogs) {
    return;
  }
  const size_t remaining =
      JavaScriptDialogInterceptor::kMaxInterceptedDialogs - target.size();
  if (followup.size() > remaining) {
    followup.resize(remaining);
  }
  base::Extend(target, std::move(followup));
}

}  // namespace

void MergeToolResult(ToolResult& target, ToolResult followup) {
  mojom::ActionResultPtr merged_result =
      std::move(target.deprecated_action_result());
  MergeActionResult(*merged_result,
                    std::move(followup.deprecated_action_result()));
  followup.set_deprecated_action_result(std::move(merged_result));
  target = std::move(followup);
}

void MergeFollowupResult(ToolResultWithLatencyInfo& target,
                         ToolResultWithLatencyInfo followup) {
  MergeToolResult(target.tool_result, std::move(followup.tool_result));
  MergeInterceptedDialogs(target.javascript_dialogs,
                          std::move(followup.javascript_dialogs));
  target.end_time = followup.end_time;
}

}  // namespace actor
