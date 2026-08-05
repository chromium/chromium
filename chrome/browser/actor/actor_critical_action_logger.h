// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_
#define CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_

#include <cstdint>
#include <string>

#include "chrome/common/actor.mojom.h"
#include "components/actor/core/task_id.h"
#include "components/critical_actions/core/browser/critical_action_types.h"
#include "services/metrics/public/cpp/ukm_source_id.h"

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
  // Evaluates whether the completed tool action is critical and, if so,
  // logs it to the CriticalActionService associated with the profile.
  static void MaybeLogAction(ActorTask& task,
                             Profile* profile,
                             const ToolRequest& action,
                             const mojom::ActionResult& result,
                             ukm::SourceId navigation_id);

  // Logs a self-reported agent action to the CriticalActionService.
  static void LogAgentSelfReportedAction(
      Profile* profile,
      std::string conversation_id,
      critical_actions::ActionType action_type,
      const GURL& url,
      ukm::SourceId navigation_id,
      TaskId actor_task_id = TaskId(),
      std::string metadata = "");

 private:
  // Helper method to populate and dispatch a CriticalActionEntry.
  static void LogEntry(critical_actions::CriticalActionService& service,
                       critical_actions::ActionType action_type,
                       std::string conversation_id,
                       TaskId actor_task_id,
                       const GURL& url,
                       std::string metadata,
                       ukm::SourceId navigation_id);
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_CRITICAL_ACTION_LOGGER_H_
