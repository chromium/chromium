// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_

#include <cstdint>
#include <string>

#include "chrome/common/actor.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/actor/core/task_source_info.h"
#include "components/critical_actions/core/browser/critical_action_types.h"

class Profile;

namespace critical_actions {
class CriticalActionService;
}  // namespace critical_actions

namespace actor {

class ActorTask;
class ToolRequest;

// Helper class to evaluate and log critical actions self-reported by Actor
// agents during tool execution.
class ActorCriticalActionLogger {
 public:
  // Returns whether critical actions of tasks created by `source` should be
  // logged. Only Glic tasks are supported for now.
  static bool IsTaskSourceSupported(TaskSourceInfo::Client source);

  // Returns whether critical actions of `task` should be logged. True if the
  // source of `task` is supported and history is recorded for `task`.
  static bool ShouldLogForTask(const ActorTask& task);

  // Evaluates whether the completed tool action is critical and, if so,
  // logs it to the CriticalActionService associated with the profile when
  // history is recorded.
  static void MaybeLogAction(ActorTask& task,
                             Profile* profile,
                             const ToolRequest& action,
                             const mojom::ActionResult& result,
                             int64_t navigation_id);

  // Logs a self-reported agent action to the `CriticalActionService`, if
  // `ShouldLogForTask()` is true for `task`.
  static void LogAgentSelfReportedAction(
      Profile* profile,
      const ActorTask& task,
      critical_actions::ActionType action_type,
      int64_t navigation_id,
      std::string metadata = "");

 private:
  // Helper method to populate and dispatch a CriticalActionEntry.
  static void LogEntry(critical_actions::CriticalActionService& service,
                       critical_actions::ActionType action_type,
                       std::string conversation_id,
                       TaskId actor_task_id,
                       std::string metadata,
                       int64_t navigation_id);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_
