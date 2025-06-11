// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
#define CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_

#include <memory>
#include <optional>

#include "base/callback_list.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/safe_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class GURL;
class Profile;

namespace mojo_base {
class ProtoWrapper;
}

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace url {
class Origin;
}  // namespace url

namespace actor {

class ActorTask;

// Coordinates the execution of a multi-step task.
class ExecutionEngine {
 public:
  using ActionResultCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  using ActionsResultCallback =
      base::OnceCallback<void(optimization_guide::proto::ActionsResult)>;

  explicit ExecutionEngine(Profile* profile);

  // Old instances of ExecutionEngine assume that all actions are scoped to a
  // single tab. This constructor supports this use case, but this is
  // deprecated. Do not add new consumers
  ExecutionEngine(Profile* profile, tabs::TabInterface* tab);
  ExecutionEngine(const ExecutionEngine&) = delete;
  ExecutionEngine& operator=(const ExecutionEngine&) = delete;
  ~ExecutionEngine();

  // This cannot be in the constructor as we first construct the
  // ExecutionEngine, then the ActorTask.
  void SetOwner(ActorTask* task);

  static void RegisterWithProfile(Profile* profile);

  // Cancels any in-progress actions with the reason: "kTaskPaused".
  void CancelOngoingActions(mojom::ActionResultCode reason);

  // Returns the tab associated with the current task if it exists.
  tabs::TabInterface* GetTabOfCurrentTask() const;

  // Returns true if a task is currently active.
  bool HasTask() const;

  // Returns true if a task is currently active in `tab`.
  bool HasTaskForTab(const content::WebContents* tab) const;

  // Performs the next action in the current task.
  void Act(const optimization_guide::proto::BrowserAction& action,
           ActionResultCallback callback);

  // Performs the next action in the current task.
  void Act(const optimization_guide::proto::Actions& actions,
           ActionsResultCallback callback);

  // Gets called when a new observation is made for the actor task.
  void DidObserveContext(const mojo_base::ProtoWrapper&);

  // Returns last observed page content, nullptr if no observation has been
  // made.
  const optimization_guide::proto::AnnotatedPageContent*
  GetLastObservedPageContent();

  // Invalidated anytime `actions_` is reset.
  base::WeakPtr<ExecutionEngine> GetWeakPtr();

 private:
  class NewTabWebContentsObserver;

  // If there are no actions remaining, calls CompleteActions.
  // Otherwise, calls SafetyChecksForNextAction().
  void KickOffNextAction(mojom::ActionResultPtr previous_action_result);

  // Performs safety checks for next action. This is asynchronous.
  void SafetyChecksForNextAction();

  // Performs synchronous safety checks for the next action. If everything
  // passes calls tool_controller_.Invoke().
  void DidFinishAsyncSafetyChecks(const url::Origin& evaluated_origin,
                                  bool may_act);

  // Synchronously executes the next action. There are several types of actions,
  // including renderer-scoped actions, tab-scoped actions, and global actions.
  void ExecuteNextAction();
  void ExecuteFrameScopedAction(
      const optimization_guide::proto::ActionInformation& action);

  // Called each time an action finishes.
  void FinishOneAction(mojom::ActionResultPtr result);

  // Calls out to CompleteActionsV1 or CompleteActionsV2.
  void CompleteActions(mojom::ActionResultPtr result);

  // Calls `callback` and clears `actions_v1_`.
  void CompleteActionsV1(mojom::ActionResultPtr result);

  // Calls `callback` and clears `actions_v2_`.
  void CompleteActionsV2(mojom::ActionResultPtr result);

  void OnTabWillDetach(tabs::TabInterface* tab,
                       tabs::TabInterface::DetachReason reason);

  const GURL& LastCommittedURLOfCurrentTask();

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  raw_ptr<Profile> profile_;
  base::SafeRef<AggregatedJournal> journal_;

  // Stores the last observed page content for TOCTOU check.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      last_observed_page_content_;

  // This is deprecated. Do not add new use cases.
  struct ActionsV1 {
    ActionsV1(const optimization_guide::proto::BrowserAction& actions,
              ActionResultCallback callback);
    ~ActionsV1();
    ActionsV1(const ActionsV1&) = delete;
    ActionsV1& operator=(const ActionsV1&) = delete;

    optimization_guide::proto::BrowserAction proto;

    // Note that ActionResultCallback != ActionsResultCallback.
    ActionResultCallback callback;
  };

  struct ActionsV2 {
    ActionsV2(const optimization_guide::proto::Actions& actions,
              ActionsResultCallback callback);
    ~ActionsV2();
    ActionsV2(const ActionsV2&) = delete;
    ActionsV2& operator=(const ActionsV2&) = delete;

    optimization_guide::proto::Actions proto;

    // Note that ActionResultCallback != ActionsResultCallback.
    ActionsResultCallback callback;
  };

  // TODO(crbug.com/411462297): This assumes all tasks are scoped to a tab,
  // which is not true. This should eventually be removed.
  bool tab_scoped_actions_deprecated_ = false;
  raw_ptr<tabs::TabInterface> tab_;
  base::CallbackListSubscription tab_will_detach_subscription_;

  // Owns `this`.
  raw_ptr<ActorTask> task_;

  ToolController tool_controller_;

  // A sequence of actions that the model has requested. When it is finished
  // being processed it is reset.
  std::optional<ActionsV1> actions_v1_;

  // A sequence of actions that the model has requested. When it is finished
  // being processed it is reset.
  std::optional<ActionsV2> actions_v2_;

  // The index of the in-progress action.
  int action_index_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);

  // Normally, a WeakPtrFactory only invalidates its WeakPtrs when the object is
  // destroyed. However, this class invalidates WeakPtrs anytime a new set of
  // actions is passed in. This effectively cancels any ongoing async actions.
  base::WeakPtrFactory<ExecutionEngine> actions_weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_EXECUTION_ENGINE_H_
