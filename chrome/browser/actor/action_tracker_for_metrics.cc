// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/action_tracker_for_metrics.h"

#include <string>

#include "base/check.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/strcat.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/actor/tools/wait_tool_request.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"

namespace actor {

ActionTrackerForMetrics::ActionTrackerForMetrics() = default;

ActionTrackerForMetrics::~ActionTrackerForMetrics() {
  int total_count = 0;
  for (const auto& [tool_name, count] : subsequent_waits_per_tool_name_) {
    base::UmaHistogramCounts100(
        base::StrCat({"Actor.Task.SubsequentWaits.", tool_name}), count);
    // We want to see how many times the observation delay was insufficient. If
    // a tool was followed by more than 1 wait actions, don't count subsequent
    // ones to avoid skewing the metrics.
    if (tool_name != WaitToolRequest::kName) {
      total_count += count;
    }
  }
  base::UmaHistogramCounts100("Actor.Task.SubsequentWaits", total_count);
}

void ActionTrackerForMetrics::WillMoveToState(ActorTask::State state) {
  switch (state) {
    case ActorTask::State::kCreated:
    case ActorTask::State::kActing:
    case ActorTask::State::kReflecting:
    case ActorTask::State::kCancelled:
    case ActorTask::State::kFinished:
    case ActorTask::State::kFailed:
      break;
    case ActorTask::State::kPausedByActor:
    case ActorTask::State::kPausedByUser:
    case ActorTask::State::kWaitingOnUser:
      last_tool_name_in_previous_sequence_if_succeeded_.clear();
      break;
  }
}

void ActionTrackerForMetrics::WillAct(
    const std::vector<std::unique_ptr<ToolRequest>>& actions) {
  // Only count if Wait is the only action.
  if (actions.size() == 1 &&
      actions.front()->Name() == WaitToolRequest::kName &&
      !last_tool_name_in_previous_sequence_if_succeeded_.empty()) {
    auto [it, _] = subsequent_waits_per_tool_name_.try_emplace(
        last_tool_name_in_previous_sequence_if_succeeded_, 0);
    it->second++;
  }

  CHECK(!actions.empty());
  last_tool_name_in_current_sequence_ = std::string(actions.back()->Name());
}

void ActionTrackerForMetrics::OnFinishedAct(const mojom::ActionResult& result) {
  if (IsOk(result)) {
    last_tool_name_in_previous_sequence_if_succeeded_ =
        last_tool_name_in_current_sequence_;
  } else {
    last_tool_name_in_previous_sequence_if_succeeded_.clear();
  }
  last_tool_name_in_current_sequence_.clear();
}

}  // namespace actor
