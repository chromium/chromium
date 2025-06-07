// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_

#include <memory>
#include <optional>

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

// Coordinates the execution of a multi-step task.
// This class is misnamed. It's a specific type of execution engine.
class ActorCoordinator {
 public:
  using ActionResultCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;

  explicit ActorCoordinator(Profile* profile);

  // Old instances of ActorCoordinator assume that all actions are scoped to a
  // single tab. This constructor supports this use case, but this is
  // deprecated. Do not add new consumers
  ActorCoordinator(Profile* profile, tabs::TabInterface* tab);
  ActorCoordinator(const ActorCoordinator&) = delete;
  ActorCoordinator& operator=(const ActorCoordinator&) = delete;
  ~ActorCoordinator();

  static void RegisterWithProfile(Profile* profile);

  // Pauses the current task, if it's active. Callbacks for in-progress actions
  // are invoked.
  void PauseTask();
  // Stop and pause are identical, they just emit different error codes.
  void StopTask();

  // Returns the tab associated with the current task if it exists.
  tabs::TabInterface* GetTabOfCurrentTask() const;

  // Returns true if a task is currently active.
  bool HasTask() const;

  // Returns true if a task is currently active in `tab`.
  bool HasTaskForTab(const content::WebContents* tab) const;

  // Performs the next action in the current task.
  void Act(const optimization_guide::proto::BrowserAction& action,
           ActionResultCallback callback);

  // Gets called when a new observation is made for the actor task.
  void DidObserveContext(const mojo_base::ProtoWrapper&);

  // Returns last observed page content, nullptr if no observation has been
  // made.
  const optimization_guide::proto::AnnotatedPageContent*
  GetLastObservedPageContent();

  base::WeakPtr<ActorCoordinator> GetWeakPtr();

 private:
  class NewTabWebContentsObserver;

  void OnMayActOnTabResponse(TaskId task_id,
                             const url::Origin& evaluated_origin,
                             bool may_act);

  // Kicks off one action from actions. If no actions are left, finishes.
  void PerformOneAction(TaskId task_id,
                        mojom::ActionResultPtr previous_action_result);
  void FinishOneAction(TaskId task_id, mojom::ActionResultPtr result);

  // Fires the callback and clears `actions`.
  void CompleteActions(mojom::ActionResultPtr result);

  const GURL& LastCommittedURLOfCurrentTask();

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  raw_ptr<Profile> profile_;
  base::SafeRef<AggregatedJournal> journal_;

  // Stores the last observed page content for TOCTOU check.
  std::unique_ptr<optimization_guide::proto::AnnotatedPageContent>
      last_observed_page_content_;

  struct Actions {
    Actions(const optimization_guide::proto::BrowserAction& actions,
            ActorCoordinator::ActionResultCallback callback);
    ~Actions();
    Actions(const Actions&) = delete;
    Actions& operator=(const Actions&) = delete;

    optimization_guide::proto::BrowserAction proto;
    ActionResultCallback callback;
  };

  // TODO(crbug.com/411462297): This assumes all tasks are scoped to a tab,
  // which is not true. This should eventually be removed.
  bool tab_scoped_actions_deprecated_ = false;
  base::WeakPtr<tabs::TabInterface> tab_;

  ToolController tool_controller_;

  // A sequence of actions that the model has requested. When it is finished
  // being processed it is reset.
  std::optional<Actions> actions_;

  // The index of the in-progress action.
  int action_index_ = 0;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ActorCoordinator> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
