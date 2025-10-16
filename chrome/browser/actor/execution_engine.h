// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
#define CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/abseil-cpp/absl/container/flat_hash_set.h"

class Profile;

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace url {
class Origin;
}  // namespace url

namespace actor {

class ActorTask;
class ToolRequest;
namespace ui {
class UiEventDispatcher;
}

// Coordinates the execution of a multi-step task.
class ExecutionEngine : public ToolDelegate {
 public:
  // State machine (success case)
  //
  //    Init
  //     |
  //     v
  // StartAction -> ToolCreateAndVerify ->
  //     ^          UiPreInvoke -> ToolInvoke -> UiPostInvoke -> Complete
  //     |                                           |              |
  //     |___________________________________________|______________|
  //
  // Complete may also be reached directly from other states in case of error.
  enum class State {
    kInit = 0,
    kStartAction,
    kToolCreateAndVerify,
    kUiPreInvoke,
    kToolInvoke,
    kUiPostInvoke,
    kComplete,
  };

  class StateObserver : public base::CheckedObserver {
   public:
    ~StateObserver() override = default;
    virtual void OnStateChanged(State old_state, State new_state) = 0;
  };

  explicit ExecutionEngine(Profile* profile);

  // Old instances of ExecutionEngine assume that all actions are scoped to a
  // single tab. This constructor supports this use case, but this is
  // deprecated. Do not add new consumers
  ExecutionEngine(Profile* profile, tabs::TabInterface* tab);
  ExecutionEngine(const ExecutionEngine&) = delete;
  ExecutionEngine& operator=(const ExecutionEngine&) = delete;
  ~ExecutionEngine() override;

  static std::unique_ptr<ExecutionEngine> CreateForTesting(
      Profile* profile,
      std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher);

  // This cannot be in the constructor as we first construct the
  // ExecutionEngine, then the ActorTask.
  void SetOwner(ActorTask* task);

  // Cancels any in-progress actions with the reason: "kTaskPaused".
  void CancelOngoingActions(mojom::ActionResultCode reason);

  // If there is an ongoing tool request, treat it as having failed with the
  // given reason.
  void FailCurrentTool(mojom::ActionResultCode reason);

  // Performs the given tool actions and invokes the callback when completed.
  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActorTask::ActCallback callback);

  // Invalidated anytime `action_sequence_` is reset.
  base::WeakPtr<ExecutionEngine> GetWeakPtr();

  // ToolDelegate:
  Profile& GetProfile() override;
  AggregatedJournal& GetJournal() override;
  favicon::FaviconService* GetFaviconService() override;
  actor_login::ActorLoginService& GetActorLoginService() override;
  void PromptToSelectCredential(
      const std::vector<actor_login::Credential>& credentials,
      const base::flat_map<std::string, gfx::Image>& icons,
      ToolDelegate::CredentialSelectedCallback callback) override;
  void SetUserSelectedCredential(
      const actor_login::Credential& credential) override;
  const std::optional<actor_login::Credential> GetUserSelectedCredential(
      const url::Origin& request_origin) const override;

  // Callback for when a credential is selected, in response to
  // `ToolDelegate::PromptToSelectCredential()`.
  void OnCredentialSelected(
      webui::mojom::SelectCredentialDialogResponsePtr response);

  using UserConfirmationDialogCallback = base::OnceCallback<void(
      webui::mojom::UserConfirmationDialogResponsePtr response)>;

  void AddWritableMainframeOrigins(
      const absl::flat_hash_set<url::Origin>& added_writable_mainframe_origins);

  void PromptToConfirmCrossOriginNavigation(
      const url::Origin& navigation_origin,
      UserConfirmationDialogCallback callback);
  void PromptToConfirmDownload(int32_t download_id,
                               UserConfirmationDialogCallback callback);

  // Callback for when the user responds to a confirmation dialog.
  void OnUserConfirmation(
      webui::mojom::UserConfirmationDialogResponsePtr response);

  static std::string StateToString(State state);

  bool ShouldGateNavigation(content::NavigationHandle& navigation_handle,
                            UserConfirmationDialogCallback callback);

  void AddObserver(StateObserver* observer);

  void RemoveObserver(StateObserver* observer);

 private:
  class NewTabWebContentsObserver;
  // Used by tests only.
  ExecutionEngine(Profile* profile,
                  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher);

  void SetState(State state);

  // Starts the next action by calling SafetyChecksForNextAction(). Must only be
  // called if there is a next action.
  void KickOffNextAction(mojom::ActionResultPtr init_hooks_result);

  // Performs safety checks for next action. This is asynchronous.
  void SafetyChecksForNextAction();

  // Performs synchronous safety checks for the next action. If everything
  // passes calls tool_controller_.Invoke().
  void DidFinishAsyncSafetyChecks(const url::Origin& evaluated_origin,
                                  bool may_act);

  // If a failure occurs before the next action starts, we associate the tab
  // that the action would have acted on with the task, so that we can provide
  // tab observations back to the client.
  void FailedOnTabBeforeToolCreation();

  // Synchronously executes the next action. There are several types of actions,
  // including renderer-scoped actions, tab-scoped actions, and global actions.
  void ExecuteNextAction();

  // Called each time an action finishes.
  void PostToolCreate(mojom::ActionResultPtr result);
  void FinishedUiPreInvoke(mojom::ActionResultPtr result);
  void FinishedToolInvoke(mojom::ActionResultPtr result);
  void FinishedUiPostInvoke(mojom::ActionResultPtr result);

  void CompleteActions(mojom::ActionResultPtr result,
                       std::optional<size_t> action_index);

  void PromptUserForConfirmationInternal(
      const std::optional<url::Origin>& navigation_origin,
      const std::optional<int32_t> download_url,
      UserConfirmationDialogCallback callback);

  // Returns the next action that will be started when ExecuteNextAction is
  // reached.
  const ToolRequest& GetNextAction() const;

  // Returns the index / action that was last executed and is still in progress.
  // It is an error to call this when an action is not in progress.
  size_t InProgressActionIndex() const;
  const ToolRequest& GetInProgressAction() const;

  void OnPromptToConfirmNavigationDecision(
      url::Origin navigation_origin,
      UserConfirmationDialogCallback callback,
      webui::mojom::UserConfirmationDialogResponsePtr response);

  bool ShouldGateNavigationInternal(
      content::NavigationHandle& navigation_handle,
      UserConfirmationDialogCallback callback);
  void LogNavigationGating(content::NavigationHandle& navigation_handle,
                           bool applied_gate);

  State state_ = State::kInit;

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  raw_ptr<Profile> profile_;
  base::SafeRef<AggregatedJournal> journal_;

  // Owns `this`.
  raw_ptr<ActorTask> task_;

  // Created when task_ is set. Handles execution details for an individual tool
  // request.
  std::unique_ptr<ToolController> tool_controller_;
  std::unique_ptr<actor_login::ActorLoginService> actor_login_service_;
  std::unique_ptr<ui::UiEventDispatcher> ui_event_dispatcher_;

  std::vector<std::unique_ptr<ToolRequest>> action_sequence_;
  ActorTask::ActCallback act_callback_;

  // The index of the next action that will be started when ExecuteNextAction is
  // reached.
  size_t next_action_index_ = 0;
  base::TimeTicks action_start_time_;

  // If set, the currently executing tool should be considered failed once it
  // completes.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason_;

  // The results for actions so far.
  std::vector<ActionResultWithLatencyInfo> action_results_;

  // Origins which the browser is allowed to navigate to under actor control
  // without prompting the user. This is applied to all navigations, including
  // those initiated by the renderer with web content.
  absl::flat_hash_set<url::Origin> allowed_navigation_origins_;

  ToolDelegate::CredentialSelectedCallback credential_selected_callback_;

  UserConfirmationDialogCallback user_confirmation_callback_;

  // For multi-step login, this is the credential that the user has chosen to
  // allow the actor to use. The key is the
  // `Credential::request_origin`.
  base::flat_map<url::Origin, actor_login::Credential>
      user_selected_credentials_;

  base::ObserverList<StateObserver> observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Normally, a WeakPtrFactory only invalidates its WeakPtrs when the object is
  // destroyed. However, this class invalidates WeakPtrs anytime a new set of
  // actions is passed in. This effectively cancels any ongoing async actions.
  base::WeakPtrFactory<ExecutionEngine> actions_weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
