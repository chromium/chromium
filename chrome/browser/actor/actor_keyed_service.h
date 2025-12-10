// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_

#include <memory>
#include <vector>

#include "base/callback_list.h"
#include "base/compiler_specific.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/page_content_annotations/multi_source_page_context_fetcher.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/buildflags.h"
#include "components/download/content/public/all_download_item_notifier.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace actor {
namespace ui {
class ActorUiStateManagerInterface;
}

class ActorPolicyChecker;
class ActorTaskMetadata;
class ToolRequest;

// This class owns all ActorTasks for a given profile. ActorTasks are kept in
// memory until the process is destroyed.
class ActorKeyedService : public KeyedService,
                          public ProfileObserver,
                          public download::AllDownloadItemNotifier::Observer {
 public:
  explicit ActorKeyedService(Profile* profile);
  ActorKeyedService(const ActorKeyedService&) = delete;
  ActorKeyedService& operator=(const ActorKeyedService&) = delete;
  ~ActorKeyedService() override;

  void Shutdown() override;

  // Convenience method, may return nullptr.
  static ActorKeyedService* Get(content::BrowserContext* context);

  // TODO(crbug.com/428014205): Create a mock ActorKeyedService for testing so
  // we can remove this function.
  void SetActorUiStateManagerForTesting(
      std::unique_ptr<ui::ActorUiStateManagerInterface> ausm);

  // Starts tracking an existing task. Returns the new task ID.
  TaskId AddActiveTask(std::unique_ptr<ActorTask> task);

  const std::map<TaskId, const ActorTask*> GetActiveTasks() const;

  std::vector<TaskId> FindTaskIdsInActive(
      base::FunctionRef<bool(const ActorTask&)> predicate) const;

  // Stop and clear all active tasks for testing only.
  void ResetForTesting();

  // Starts a new task with an execution engine and returns the new task's id.
  // `options`, when provided, contains information used to initialize the
  // task.
  TaskId CreateTask();
  TaskId CreateTaskWithOptions(webui::mojom::TaskOptionsPtr options,
                               base::WeakPtr<ActorTaskDelegate> delegate);

  // Executes the given ToolRequest actions using the execution engine for the
  // given task id.
  using PerformActionsCallback = base::OnceCallback<void(
      mojom::ActionResultCode /*result_code*/,
      std::optional<size_t> /*index_of_failing_action*/,
      std::vector<ActionResultWithLatencyInfo> /* action_results */)>;
  void PerformActions(TaskId task_id,
                      std::vector<std::unique_ptr<ToolRequest>>&& actions,
                      ActorTaskMetadata task_metadata,
                      PerformActionsCallback callback);

  // Stops a task by its ID, `stop_reason` determines the reason for the task
  // stopping.
  void StopTask(TaskId task_id, ActorTask::StoppedReason stop_reason);

  // Returns the task with the given ID. Returns nullptr if the task does not
  // exist.
  ActorTask* GetTask(TaskId task_id);

  // The associated journal for the associated profile.
  AggregatedJournal& GetJournal() LIFETIME_BOUND { return journal_; }

  // The associated ActorUiStateManager for the associated profile.
  ui::ActorUiStateManagerInterface* GetActorUiStateManager();

  ActorPolicyChecker& GetPolicyChecker();

  // Returns true if there is a task that is actively (i.e. not paused) acting
  // in the given `tab`.
  bool IsActiveOnTab(const tabs::TabInterface& tab) const;

  // Returns the id of an ActorTask which has the given tab in its set. Returns
  // a null TaskId if no task has `tab`. Note: a returned task may be paused.
  TaskId GetTaskFromTab(const tabs::TabInterface& tab) const;

  Profile* GetProfile();

  using TabObservationResult =
      page_content_annotations::FetchPageContextResultCallbackArg;
  // Request a TabObservation be generated from the given tab.
  void RequestTabObservation(
      tabs::TabInterface& tab,
      TaskId task_id,
      base::OnceCallback<void(TabObservationResult)> callback);

  // A TabObservationResult may return the successful side of the base::expected
  // but the partial errors in the FetchPageContextResult may be considered a
  // failing result for actor. Returns a failing string in any case the result
  // isn't usable for actor. Returns nullopt if the result is fully successful.
  static std::optional<std::string> ExtractErrorMessageIfFailed(
      const TabObservationResult& result);

  using TaskStateChangedCallback =
      base::RepeatingCallback<void(TaskId, ActorTask::State)>;
  base::CallbackListSubscription AddTaskStateChangedCallback(
      TaskStateChangedCallback callback);

  void NotifyTaskStateChanged(TaskId task_id, ActorTask::State state);
  void OnActOnWebCapabilityChanged(bool can_act_on_web);

  using ActOnWebCapabilityChangedCallback = base::RepeatingCallback<void(bool)>;
  base::CallbackListSubscription AddActOnWebCapabilityChangedCallback(
      ActOnWebCapabilityChangedCallback callback);

  // Returns the acting task for web_contents. Returns nullptr if acting task
  // does not exist.
  const ActorTask* GetActingActorTaskForWebContents(
      content::WebContents* web_contents);

  using CreateActorTabCallback = base::OnceCallback<void(tabs::TabInterface*)>;
  void CreateActorTab(TaskId task_id,
                      bool open_in_background,
                      tabs::TabHandle initiator_tab_handle,
                      SessionID initiator_window_id,
                      CreateActorTabCallback callback);

  // download::AllDownloadItemNotifier::Observer
  void OnDownloadCreated(content::DownloadManager* manager,
                         download::DownloadItem* item) override;

  // Once the profile is initialized, we can get the DownloadManager for the
  // download::AllDownloadItemNotifier::Observer
  void OnProfileInitializationComplete(Profile* profile) override;

  base::WeakPtr<ActorKeyedService> GetWeakPtr();

 private:
  // The callback used for ExecutorEngine::Act.
  void OnActionsFinished(
      PerformActionsCallback callback,
      actor::mojom::ActionResultPtr action_result,
      std::optional<size_t> index_of_failed_action,
      std::vector<ActionResultWithLatencyInfo> action_results);

  // Stops all the active tasks.
  void StopAllTasks(ActorTask::StoppedReason stop_reason);

  // The jounrnal should be last in destruction order since other things like
  // ActorTask might be using a SafeRef to this object.
  AggregatedJournal journal_;

  // download notifier for metrics and the profile observer to help set up the
  // download notifier.
  std::unique_ptr<download::AllDownloadItemNotifier> download_notifier_;
  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};

  // Needs to be declared before the tasks, as they will indirectly have a
  // reference to it. This ensures the correct destruction order.
  std::unique_ptr<ui::ActorUiStateManagerInterface> actor_ui_state_manager_;

  std::map<TaskId, std::unique_ptr<ActorTask>> active_tasks_;

  TaskId::Generator next_task_id_;

  std::unique_ptr<ActorPolicyChecker> policy_checker_;

  base::RepeatingCallbackList<void(TaskId, ActorTask::State)>
      tab_state_change_callback_list_;

  base::RepeatingCallbackList<ActOnWebCapabilityChangedCallback::RunType>
      act_on_web_capability_changed_callback_list_;

  // Owns this.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedService> weak_ptr_factory_{this};
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
