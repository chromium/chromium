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
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/password_manager/actor_login/actor_login_service.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace mojo_base {
class ProtoWrapper;
}

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

  static void RegisterWithProfile(Profile* profile);

  // Cancels any in-progress actions with the reason: "kTaskPaused".
  void CancelOngoingActions(mojom::ActionResultCode reason);

  // If there is an ongoing tool request, treat it as having failed with the
  // given reason.
  void FailCurrentTool(mojom::ActionResultCode reason);

  // Performs the given tool actions and invokes the callback when completed.
  void Act(std::vector<std::unique_ptr<ToolRequest>>&& actions,
           ActorTask::ActCallback callback);

  // Gets called when a new observation is made for the actor task.
  void DidObserveContext(const mojo_base::ProtoWrapper&);

  // Returns last observed page content, nullptr if no observation has been
  // made.
  const optimization_guide::proto::AnnotatedPageContent*
  GetLastObservedPageContent();

  // Invalidated anytime `action_sequence_` is reset.
  base::WeakPtr<ExecutionEngine> GetWeakPtr();

  // ToolDelegate:
  AggregatedJournal& GetJournal() override;
  actor_login::ActorLoginService& GetActorLoginService() override;

  void SetActorLoginServiceForTesting(
      std::unique_ptr<actor_login::ActorLoginService> test_service);

  static std::string StateToString(State state);

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

  // Returns the next action that will be started when ExecuteNextAction is
  // reached.
  const ToolRequest& GetNextAction() const;

  // Returns the index / action that was last executed and is still in progress.
  // It is an error to call this when an action is not in progress.
  size_t InProgressActionIndex() const;
  const ToolRequest& GetInProgressAction() const;

  State state_ = State::kInit;

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  raw_ptr<Profile> profile_;
  base::SafeRef<AggregatedJournal> journal_;

  // Stores the last observed page content for TOCTOU check.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      last_observed_page_content_;

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

  // If set, the currently executing tool should be considered failed once it
  // completes.
  std::optional<mojom::ActionResultCode> external_tool_failure_reason_;

  SEQUENCE_CHECKER(sequence_checker_);

  // Normally, a WeakPtrFactory only invalidates its WeakPtrs when the object is
  // destroyed. However, this class invalidates WeakPtrs anytime a new set of
  // actions is passed in. This effectively cancels any ongoing async actions.
  base::WeakPtrFactory<ExecutionEngine> actions_weak_ptr_factory_{this};
};

std::ostream& operator<<(std::ostream& o, const ExecutionEngine::State& s);

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
