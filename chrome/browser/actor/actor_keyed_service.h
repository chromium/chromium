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
#include "chrome/browser/actor/actor_task_delegate.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom-forward.h"
#include "chrome/common/buildflags.h"
#include "components/keyed_service/core/keyed_service.h"
#include "components/password_manager/core/browser/actor_login/actor_login_types.h"
#include "components/tabs/public/tab_interface.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

class Profile;

namespace content {
class BrowserContext;
}  // namespace content

namespace page_content_annotations {
struct FetchPageContextResult;
}  // namespace page_content_annotations

namespace actor {
namespace ui {
class ActorUiStateManagerInterface;
}

class ActorPolicyChecker;
class ActorTask;
class ActorTaskMetadata;
class ActorTaskDelegate;
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

  std::vector<TaskId> FindTaskIdsInActive(
      base::FunctionRef<bool(const ActorTask&)> predicate) const;
  std::vector<TaskId> FindTaskIdsInInactive(
      base::FunctionRef<bool(const ActorTask&)> predicate) const;

  // Stop and clear all active and inactive tasks for testing only.
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

  // Stops a task by its ID, `success` determines if the task was finished
  // successfully or ended early.
  void StopTask(TaskId task_id, bool success);

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

  using TabObservationResult = base::expected<
      std::unique_ptr<page_content_annotations::FetchPageContextResult>,
      std::string>;

  // Request a TabObservation be generated from the given tab.
  void RequestTabObservation(
      tabs::TabInterface& tab,
      TaskId task_id,
      base::OnceCallback<void(TabObservationResult)> callback);

  using TaskStateChangedCallback =
      base::RepeatingCallback<void(const ActorTask&)>;
  base::CallbackListSubscription AddTaskStateChangedCallback(
      TaskStateChangedCallback callback);

  void NotifyTaskStateChanged(const ActorTask& task);

  // Allows the subscribers to get notified when a credential selection prompt
  // is requested.
  using CredentialSelectedCallback = base::RepeatingCallback<void(
      webui::mojom::SelectCredentialDialogResponsePtr)>;
  using RequestToShowCredentialSelectionDialogSubscriberCallback =
      base::RepeatingCallback<void(
          TaskId,
          const base::flat_map<std::string, gfx::Image>& icons,
          const std::vector<actor_login::Credential>&,
          CredentialSelectedCallback)>;
  base::CallbackListSubscription
  AddRequestToShowCredentialSelectionDialogSubscriberCallback(
      RequestToShowCredentialSelectionDialogSubscriberCallback callback);

  // Notifies the subscribers that a credential selection prompt is requested
  // for the given task.
  void NotifyRequestToShowCredentialSelectionDialog(
      TaskId task_id,
      const base::flat_map<std::string, gfx::Image>& icons,
      const std::vector<actor_login::Credential>& credentials);

  // Callback for when a credential is selected.
  void OnCredentialSelected(
      TaskId request_task_id,
      webui::mojom::SelectCredentialDialogResponsePtr response);

  using UserConfirmationDialogCallback = base::RepeatingCallback<void(
      webui::mojom::UserConfirmationDialogResponsePtr)>;
  using RequestToShowUserConfirmationDialogSubscriberCallback =
      base::RepeatingCallback<void(const std::optional<url::Origin>&,
                                   const std::optional<int32_t>,
                                   UserConfirmationDialogCallback)>;

  base::CallbackListSubscription
  AddRequestToShowUserConfirmationDialogSubscriberCallback(
      RequestToShowUserConfirmationDialogSubscriberCallback callback);

  // Notifies the subscribers that the browser is requesting user confirmation
  // for the actor to continue.
  void NotifyRequestToShowUserConfirmationDialog(
      TaskId task_id,
      const std::optional<url::Origin>& navigation_origin,
      const std::optional<int32_t> download_id);

  void OnUserConfirmationDialogDecision(
      TaskId request_task_id,
      webui::mojom::UserConfirmationDialogResponsePtr response);

  void OnActuationCapabilityChanged(bool has_actuation_capability);

  // Returns the acting task for web_contents. Returns nullptr if acting task
  // does not exist.
  const ActorTask* GetActingActorTaskForWebContents(
      content::WebContents* web_contents);

  base::WeakPtr<ActorKeyedService> GetWeakPtr();

 private:
  // The callback used for ExecutorEngine::Act.
  void OnActionsFinished(
      PerformActionsCallback callback,
      actor::mojom::ActionResultPtr action_result,
      std::optional<size_t> index_of_failed_action,
      std::vector<ActionResultWithLatencyInfo> action_results);

  // Fails all the active tasks.
  void FailAllTasks();

  // Needs to be declared before the tasks, as they will indirectly have a
  // reference to it. This ensures the correct destruction order.
  std::unique_ptr<ui::ActorUiStateManagerInterface> actor_ui_state_manager_;

  std::map<TaskId, std::unique_ptr<ActorTask>> active_tasks_;
  // Stores completed tasks. May want to add cancelled tasks in the future.
  std::map<TaskId, std::unique_ptr<ActorTask>> inactive_tasks_;

  TaskId::Generator next_task_id_;

  AggregatedJournal journal_;

  std::unique_ptr<ActorPolicyChecker> policy_checker_;

  base::RepeatingCallbackList<void(const ActorTask&)>
      tab_state_change_callback_list_;

  base::RepeatingCallbackList<
      RequestToShowCredentialSelectionDialogSubscriberCallback::RunType>
      request_to_show_credential_selection_dialog_callback_list_;

  base::RepeatingCallbackList<
      RequestToShowUserConfirmationDialogSubscriberCallback::RunType>
      request_to_show_user_confirmation_dialog_callback_list_;

  // Owns this.
  raw_ptr<Profile> profile_;

  base::WeakPtrFactory<ActorKeyedService> weak_ptr_factory_{this};
};

}  // namespace actor
#endif  // CHROME_BROWSER_ACTOR_ACTOR_KEYED_SERVICE_H_
