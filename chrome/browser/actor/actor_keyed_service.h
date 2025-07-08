// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/tabs/public/tab_interface.h"

#if BUILDFLAG(ENABLE_GLIC)
#include "chrome/browser/glic/host/glic.mojom-forward.h"
#include "chrome/common/actor.mojom-forward.h"
#endif

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace actor {
namespace ui {
class ActorUiStateManagerInterface;
}

// This class owns all ActorTasks for a given profile. ActorTasks are kept in
// memory until the process is destroyed.
class ActorKeyedService : public KeyedService {
 public:
  explicit ActorKeyedService(
      Profile* profile,
      std::unique_ptr<ui::ActorUiStateManagerInterface> ui_state_manager);
  ActorKeyedService(const ActorKeyedService&) = delete;
  ActorKeyedService& operator=(const ActorKeyedService&) = delete;
  ~ActorKeyedService() override;

  // Convenience method, may return nullptr.
  static ActorKeyedService* Get(content::BrowserContext* context);

  // Starts tracking an existing task. Returns the new task ID.
  TaskId AddActiveTask(std::unique_ptr<ActorTask> task);

  const std::map<TaskId, const ActorTask*> GetActiveTasks() const;
  const std::map<TaskId, const ActorTask*> GetInactiveTasks() const;

  // Starts a new task with an execution engine and returns the new task's id.
  TaskId CreateTask();

  // Executes the given actions using the execution engine. The actions proto
  // must explicitly specify the task_id of an existing task started using
  // CreateTask. Once all actions have been completed, returns the ActionsResult
  // proto which includes new observations and an error code for the first
  // failed action.
  // TODO(crbug.com/411462297): The result doesn't yet include observations.
  void PerformActions(
      optimization_guide::proto::Actions actions,
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>
          callback);

  // TODO(crbug.com/411462297): DEPRECATED - to be replaced with PerformActions.
  // Executes an actor action.
  void ExecuteAction(
      optimization_guide::proto::BrowserAction action,
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback);

  // TODO(crbug.com/411462297): DEPRECATED - to be replaced with CreateTask
  // above.
  // Starts a new task using the execution engine and fires
  // `callback` when the task is ready. Implicitly calls AddTask.
  void StartTask(
      optimization_guide::proto::BrowserStartTask task,
      base::OnceCallback<
          void(optimization_guide::proto::BrowserStartTaskResult)> callback);

  // Stops a task by its ID.
  void StopTask(TaskId task_id);

  // Returns the task with the given ID. Returns nullptr if the task does not
  // exist.
  ActorTask* GetTask(TaskId task_id);

  // TODO(crbug.com/411462297): This is a temporary shim to allow removing
  // GlicActorController's notion of "current task". Eventually all actions will
  // supply a task id.
  ActorTask* GetMostRecentTask();

  // The associated journal for the associated profile.
  AggregatedJournal& GetJournal() LIFETIME_BOUND { return journal_; }

  // The associated ActorUiStateManager for the associated profile.
  ui::ActorUiStateManagerInterface* GetActorUiStateManager();

  // Called whenever an actor task state changes.
  void OnActorTaskStateChanged(TaskId task_id, ActorTask::State task_state);

  bool IsAnyTaskActingOnTab(const tabs::TabInterface& tab) const;

 private:
  // Start task is currently asynchronous.
  // TODO(crbug.com/411462297): This is a short term hack. Eventually StartTask
  // will become synchronous.
  void FinishStartTask(
      tabs::TabHandle handle,
      base::OnceCallback<
          void(optimization_guide::proto::BrowserStartTaskResult)> callback);

#if BUILDFLAG(ENABLE_GLIC)
  void ConvertToBrowserActionResult(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      int task_id,
      int32_t tab_id,
      actor::mojom::ActionResultPtr action_result,
      glic::mojom::GetContextResultPtr result);
  // Called when the actor coordinator has finished an action which required
  // task creation.
  void OnActionFinished(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      int task_id,
      actor::mojom::ActionResultPtr action_result);
#endif

  void OnActionsFinished(
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>
          callback,
      actor::mojom::ActionResultPtr action_result);

  std::map<TaskId, std::unique_ptr<ActorTask>> active_tasks_;
  // Stores completed tasks. May want to add cancelled tasks in the future.
  std::map<TaskId, std::unique_ptr<ActorTask>> inactive_tasks_;

  std::unique_ptr<ui::ActorUiStateManagerInterface> actor_ui_state_manager_;

  // Holds subscriptions for ActorTask callbacks.
  std::vector<base::CallbackListSubscription> actor_task_subscriptions_;

  TaskId::Generator next_task_id_;

  AggregatedJournal journal_;

  // TODO(crbug.com/411462297): Remove
  TaskId last_created_task_id_;

  // Owns this.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedService> weak_ptr_factory_{this};
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
