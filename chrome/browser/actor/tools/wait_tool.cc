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

namespace actor {

bool WaitTool::no_delay_for_testing_ = false;

WaitTool::WaitTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   base::TimeDelta wait_duration,
                   ActorSurfaceHandle observe_actor_surface_handle)
    : Tool(task_id, tool_delegate),
      wait_duration_(wait_duration),
      observe_actor_surface_handle_(observe_actor_surface_handle) {}

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
  if (!observe_actor_surface_handle_.is_null()) {
    task.ObserveActorSurfaceOnce(observe_actor_surface_handle_);
  }

  std::move(callback).Run(MakeOkResult());
}

ActorSurfaceHandle WaitTool::GetTargetActorSurface() const {
  return observe_actor_surface_handle_;
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
