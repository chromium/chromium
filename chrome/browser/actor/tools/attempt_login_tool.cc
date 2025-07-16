// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/attempt_login_tool.h"

#include "base/notimplemented.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

}  // namespace

AttemptLoginTool::AttemptLoginTool(TaskId task_id,
                                   AggregatedJournal& journal,
                                   tabs::TabInterface& tab)
    : Tool(task_id, journal), tab_handle_(tab.GetHandle()) {}

AttemptLoginTool::~AttemptLoginTool() = default;

void AttemptLoginTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void AttemptLoginTool::Invoke(InvokeCallback callback) {
  // TODO(crbug.com/427817201): Implement the tool.
  NOTIMPLEMENTED();
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string AttemptLoginTool::DebugString() const {
  return "AttemptLoginTool";
}

std::string AttemptLoginTool::JournalEvent() const {
  return "AttemptLogin";
}

std::unique_ptr<ObservationDelayController>
AttemptLoginTool::GetObservationDelayer() const {
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_));
}

void AttemptLoginTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              InvokeCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

}  // namespace actor
