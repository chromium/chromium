// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tool.h"

#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

Tool::Tool(TaskId task_id, ToolDelegate& tool_delegate)
    : task_id_(task_id), tool_delegate_(tool_delegate) {}
Tool::~Tool() = default;

mojom::ActionResultPtr Tool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  // TODO(crbug.com/411462297): This should be made pure-virtual.
  return MakeOkResult();
}

GURL Tool::JournalURL() const {
  return GURL::EmptyGURL();
}

void Tool::UpdateTaskBeforeInvoke(ActorTask& task,
                                  InvokeCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(MakeOkResult());
}

void Tool::UpdateTaskAfterInvoke(ActorTask& task,
                                 InvokeCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(MakeOkResult());
}

}  // namespace actor
