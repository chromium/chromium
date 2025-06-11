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

// This class owns all ActorTasks for a given profile. ActorTasks are kept in
// memory until the process is destroyed.
class ActorKeyedService : public KeyedService {
 public:
  explicit ActorKeyedService(Profile* profile);
  ActorKeyedService(const ActorKeyedService&) = delete;
  ActorKeyedService& operator=(const ActorKeyedService&) = delete;
  ~ActorKeyedService() override;

  // Convenience method, may return nullptr.
  static ActorKeyedService* Get(content::BrowserContext* context);

  // Starts tracking an existing task. Returns the new task ID.
  TaskId AddTask(std::unique_ptr<ActorTask> task);

  // In the future we may want to return a more limited or const-version of
  // ActorTasks. The purpose of this method is to get information about tasks,
  // not to modify them.
  const std::map<TaskId, std::unique_ptr<ActorTask>>& GetTasks();

  // Executes an actor action. The first action in a task must be navigate.
  void ExecuteAction(
      optimization_guide::proto::BrowserAction action,
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback);

  // Starts a new task and fires `callback` when the task is ready. Implicitly
  // calls AddTask.
  void PerformActions(
      optimization_guide::proto::Actions actions,
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>
          callback);

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

  // The associated journal for the associated profile.
  AggregatedJournal& GetJournal() LIFETIME_BOUND { return journal_; }

 private:
  // Start task is currently asynchronous.
  // TODO(crbug.com/411462297): This is a short term hack. Eventually StartTask
  // will become synchronous.
  void FinishStartTask(
      tabs::TabHandle handle,
      optimization_guide::proto::BrowserStartTask task,
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
      optimization_guide::proto::ActionsResult result);

  // In the future we may want to divide this between active and inactive tasks.
  std::map<TaskId, std::unique_ptr<ActorTask>> tasks_;

  TaskId::Generator next_task_id_;

  AggregatedJournal journal_;

  // Owns this.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedService> weak_ptr_factory_{this};
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
