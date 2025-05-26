// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_

#include <memory>
#include <optional>

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/types/id_type.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/actor/tools/tool_controller.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/web_contents_observer.h"

class Profile;

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace url {
class Origin;
}  // namespace url

namespace optimization_guide::proto {
class BrowserAction;
}  // namespace optimization_guide::proto

namespace actor {

// Coordinates the execution of a multi-step task.
// This class is misnamed. It's a specific type of execution engine.
class ActorCoordinator {
 public:
  using ActionResultCallback = base::OnceCallback<void(mojom::ActionResultPtr)>;
  using StartTaskCallback =
      base::OnceCallback<void(base::WeakPtr<tabs::TabInterface>)>;

  explicit ActorCoordinator(Profile* profile);
  ActorCoordinator(const ActorCoordinator&) = delete;
  ActorCoordinator& operator=(const ActorCoordinator&) = delete;
  ~ActorCoordinator();

  // TODO(crbug.com/409564704): This is temporary. The action_observation_delay_
  // is a temporary solution that simply waits a static amount of time after a
  // tool is invoked before an observation is captured. In the future, the actor
  // framework will be smarter about when an observation should be made but for
  // now ensure the page is given some time to react to the tool invocation.
  static void SetActionObservationDelayForTesting(const base::TimeDelta& delay);
  static base::TimeDelta GetActionObservationDelay();

  static void RegisterWithProfile(Profile* profile);

  // Starts a new task.
  // Currently, requires a navigate action to start.
  // If starting the task succeeds, provides the tab in the callback, otherwise
  // null. Starting the task may fail for any of:
  //   - The `action` is not navigate.
  //   - There is already a task started, or attempting to create a new tab to
  //   start a task.
  //   - If a tab handle is provided, the tab must exist and be valid. The task
  //   will fail if the tab cannot be found or is invalid.
  //   - If no tab handle is provided, a new tab will be created.
  void StartTask(const optimization_guide::proto::BrowserAction& action,
                 StartTaskCallback callback,
                 std::optional<tabs::TabHandle> tab_handle);

  // Stops the current task, if it's active. Callbacks for
  // in-progress actions are invoked.
  void StopTask();
  // Pauses the current task, if it's active. Callbacks for in-progress actions
  // are invoked.
  void PauseTask();

  // Returns the tab associated with the current task if it exists.
  tabs::TabInterface* GetTabOfCurrentTask() const;

  // Returns true if a task is currently active.
  bool HasTask() const;

  // Returns true if a task is currently active in `tab`.
  bool HasTaskForTab(const content::WebContents* tab) const;

  // Starts new task with an existing tab, for testing only. Intended for unit
  // tests that do not use a browser and actual navigation.
  void StartTaskForTesting(tabs::TabInterface* tab);

  // Performs the next action in the current task.
  // The task must have been started by first calling `StartTask()`.
  void Act(const optimization_guide::proto::BrowserAction& action,
           ActionResultCallback callback);

 private:
  class NewTabWebContentsObserver;

  // Starts a new task, after validating there isn't already a task being
  // initialized or in progress.
  void TryStartNewTask(const optimization_guide::proto::BrowserAction& action,
                       StartTaskCallback callback,
                       std::optional<tabs::TabHandle> tab_handle);

  // Invokes the StartTask callback when initializing a new task failed (e.g.
  // error creating a new tab). Must be called to reset from the "initializing"
  // state.
  void PostTaskForStartInitializationFailed(
      ActorCoordinator::StartTaskCallback callback);

  // Creates a new tab to be used for performing a task.
  void CreateNewTab(StartTaskCallback callback);

  void OnNewTabCreated(StartTaskCallback callback,
                       content::WebContents* web_contents);

  void OnMayActOnTabResponse(TaskId task_id,
                             const url::Origin& evaluated_origin,
                             bool may_act);

  void CompleteAction(mojom::ActionResultPtr result);

  base::WeakPtr<ActorCoordinator> GetWeakPtr();

  static std::optional<base::TimeDelta> action_observation_delay_for_testing_;

  bool initializing_new_task_ = false;
  raw_ptr<Profile> profile_;

  struct Action {
    Action(const optimization_guide::proto::BrowserAction& action,
           ActorCoordinator::ActionResultCallback callback);
    ~Action();
    Action(const Action&) = delete;
    Action& operator=(const Action&) = delete;

    optimization_guide::proto::BrowserAction proto;
    ActionResultCallback callback;
  };

  // In order to perform actions, the client must start a "task". A task is
  // associated with a single tab that cannot change. Only a single task can be
  // active at a time.
  struct Task {
    explicit Task(tabs::TabInterface& task_tab);
    ~Task();
    Task(const Task&) = delete;
    Task& operator=(const Task&) = delete;

    TaskId id;

    // TODO(mcnee): Ensure this task can't outlive the tab, then stop using weak
    // ptr.
    base::WeakPtr<tabs::TabInterface> tab;
    ToolController tool_controller;

    std::optional<Action> current_action;

    bool HasTab() const { return !!tab; }

    bool HasAction() const { return !!current_action; }

   private:
    static TaskId::Generator id_generator_;
  };
  std::unique_ptr<Task> task_state_;

  std::unique_ptr<NewTabWebContentsObserver> new_tab_web_contents_observer_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ActorCoordinator> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
