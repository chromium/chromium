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
  return MakeOkResult();
}

GURL Tool::JournalURL() const {
  return GURL::EmptyGURL();
}

void Tool::UpdateTaskBeforeInvoke(ActorTask& task,
                                  ToolCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(MakeOkResult());
}

void Tool::UpdateTaskAfterInvoke(ActorTask& task,
                                 mojom::ActionResultPtr result,
                                 ToolCallback callback) const {
  // Do nothing by default, just trigger the callback.
  std::move(callback).Run(std::move(result));
}

}  // namespace actor
