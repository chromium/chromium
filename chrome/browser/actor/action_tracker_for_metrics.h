// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTION_TRACKER_FOR_METRICS_H_
#define CHROME_BROWSER_ACTOR_ACTION_TRACKER_FOR_METRICS_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/common/actor.mojom-forward.h"

namespace actor {

class ToolRequest;

// A helper class owned by `ActorTask` that tracks the tool actions executed
// within a task for the purpose of recording UMA metrics.
class ActionTrackerForMetrics {
 public:
  ActionTrackerForMetrics();
  ~ActionTrackerForMetrics();

  void WillMoveToState(ActorTask::State state);

  void WillAct(const std::vector<std::unique_ptr<ToolRequest>>& actions);

  void OnFinishedAct(const mojom::ActionResult& result);

 private:
  // Caches the last tool name found in the current sequence. This is only
  // committed to the persistent state if the sequence succeeded.
  std::string last_tool_name_in_current_sequence_;

  // The name of he last tool from the previous successful `ActorTask::Act()`
  // call since the task was last created or resumed. This state is transient
  // and is reset whenever the task is paused or interrupted, or if a sequence
  // failed.
  std::string last_tool_name_in_previous_sequence_if_succeeded_;

  // A map from the name of a tool request to the number of times a wait action
  // was executed immediately after it within this task's lifetime.
  std::map<std::string, int> subsequent_waits_per_tool_name_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTION_TRACKER_FOR_METRICS_H_
