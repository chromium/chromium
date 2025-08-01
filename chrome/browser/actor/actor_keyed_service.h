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
#include "base/types/expected.h"
#include "build/build_config.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace actor {
namespace ui {
class ActorUiStateManagerInterface;
}

class ToolRequest;

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

  // TODO(crbug.com/428014205): Create a mock ActorKeyedService for testing so
  // we can remove this function.
  void SetActorUiStateManagerForTesting(
      std::unique_ptr<ui::ActorUiStateManagerInterface> ausm);

  // Starts tracking an existing task. Returns the new task ID.
  TaskId AddActiveTask(std::unique_ptr<ActorTask> task);

  const std::map<TaskId, const ActorTask*> GetActiveTasks() const;
  const std::map<TaskId, const ActorTask*> GetInactiveTasks() const;

  // Stop and clear all active and inactive tasks for testing only.
  void ResetForTesting();

  // Starts a new task with an execution engine and returns the new task's id.
  TaskId CreateTask();

  // Executes the given ToolRequest actions using the execution engine for the
  // given task id.
  using PerformActionsCallback = base::OnceCallback<void(
      mojom::ActionResultCode /*result_code*/,
      std::optional<size_t> /*index_of_failing_action*/)>;
  void PerformActions(TaskId task_id,
                      std::vector<std::unique_ptr<ToolRequest>>&& actions,
                      PerformActionsCallback callback);

  // TODO(crbug.com/411462297): DEPRECATED - to be replaced with PerformActions.
  // Executes an actor action.
  void ExecuteAction(
      TaskId task_id,
      std::vector<std::unique_ptr<ToolRequest>>&& actions,
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback);

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

  bool IsAnyTaskActingOnTab(const tabs::TabInterface& tab) const;
  Profile* GetProfile();

  using TabObservationResult = base::expected<
      std::unique_ptr<page_content_annotations::FetchPageContextResult>,
      std::string>;

  // Request a TabOservation be generated from the given tab.
  void RequestTabObservation(
      const tabs::TabInterface& tab,
      base::OnceCallback<void(TabObservationResult)> callback);

  using TaskStateChangedCallback =
      base::RepeatingCallback<void(const ActorTask&)>;
  base::CallbackListSubscription AddTaskStateChangedCallback(
      TaskStateChangedCallback callback);

  void NotifyTaskStateChanged(const ActorTask& task);

  // Returns the acting task for web_contents. Returns nullptr if acting task
  // does not exist.
  const ActorTask* GetActingActorTaskForWebContents(
      content::WebContents* web_contents);

 private:
  // Called when the actor coordinator has finished an action which required
  // task creation.
  void OnActionFinished(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      TaskId task_id,
      actor::mojom::ActionResultPtr action_result,
      std::optional<size_t> index_of_failed_action);

  // The callback used for ExecutorEngine::Act.
  void OnActionsFinished(PerformActionsCallback callback,
                         actor::mojom::ActionResultPtr action_result,
                         std::optional<size_t> index_of_failed_action);

  void ConvertToBrowserActionResult(
      base::OnceCallback<void(optimization_guide::proto::BrowserActionResult)>
          callback,
      TaskId task_id,
      int32_t tab_id,
      const GURL& url,
      actor::mojom::ActionResultPtr action_result,
      TabObservationResult context_result);
  void OnTabOservationResult(
      base::OnceCallback<void(TabObservationResult)> callback,
      base::expected<
          std::unique_ptr<page_content_annotations::FetchPageContextResult>,
          std::string> result);

  // Needs to be declared before the tasks, as they will indirectly have a
  // reference to it. This ensures the correct destruction order.
  std::unique_ptr<ui::ActorUiStateManagerInterface> actor_ui_state_manager_;

  std::map<TaskId, std::unique_ptr<ActorTask>> active_tasks_;
  // Stores completed tasks. May want to add cancelled tasks in the future.
  std::map<TaskId, std::unique_ptr<ActorTask>> inactive_tasks_;

  TaskId::Generator next_task_id_;

  AggregatedJournal journal_;

  base::RepeatingCallbackList<void(const ActorTask&)>
      tab_state_change_callback_list_;

  // TODO(crbug.com/411462297): Remove
  TaskId last_created_task_id_;

  // Owns this.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedService> weak_ptr_factory_{this};
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
