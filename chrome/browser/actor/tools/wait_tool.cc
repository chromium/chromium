// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/wait_tool.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

bool WaitTool::no_delay_for_testing_ = false;

WaitTool::WaitTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   base::TimeDelta wait_duration,
                   tabs::TabHandle observe_tab_handle)
    : Tool(task_id, tool_delegate),
      wait_duration_(wait_duration),
      observe_tab_handle_(observe_tab_handle) {}

WaitTool::~WaitTool() = default;

void WaitTool::Validate(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void WaitTool::Invoke(ToolCallback callback) {
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&WaitTool::OnDelayFinished, weak_ptr_factory_.GetWeakPtr(),
                     std::move(callback)),
      no_delay_for_testing_ ? base::TimeDelta() : wait_duration_);
}

std::string WaitTool::DebugString() const {
  return "WaitTool";
}

std::string WaitTool::JournalEvent() const {
  return "Wait";
}

std::unique_ptr<ObservationDelayController> WaitTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // Wait tool shouldn't delay observation aside from its own built-in delay.
  return nullptr;
}

void WaitTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                      ToolCallback callback) const {
  if (observe_tab_handle_ != tabs::TabHandle::Null()) {
    task.ObserveTabOnce(observe_tab_handle_);
  }

  std::move(callback).Run(MakeOkResult());
}

tabs::TabHandle WaitTool::GetTargetTab() const {
  return tabs::TabHandle::Null();
}

void WaitTool::OnDelayFinished(ToolCallback callback) {
  // TODO(crbug.com/409566732): Add more robust methods for detecting that the
  // page has settled.
  std::move(callback).Run(MakeOkResult());
}

// static
void WaitTool::SetNoDelayForTesting() {
  no_delay_for_testing_ = true;
}

}  // namespace actor
