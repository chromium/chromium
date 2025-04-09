// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
#define CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_

#include <memory>

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "chrome/browser/actor/tools/tool_controller.h"
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
class ActorCoordinator {
 public:
  using ActionResultCallback = base::OnceCallback<void(bool)>;
  using StartTaskCallback =
      base::OnceCallback<void(base::WeakPtr<tabs::TabInterface>)>;

  explicit ActorCoordinator(Profile* profile);
  ActorCoordinator(const ActorCoordinator&) = delete;
  ActorCoordinator& operator=(const ActorCoordinator&) = delete;
  ~ActorCoordinator();

  static void RegisterWithProfile(Profile* profile);

  // Starts a new task.
  // Currently, requires a navigate action to start, and always creates a new
  // tab.
  // If starting the task succeeds, provides the newly-created tab in the
  // callback, otherwise null.
  // Starting the task may fail for any of:
  //   - The `action` is not navigate.
  //   - There is already a task started, or attempting to create a new tab to
  //   start a task.
  //   - Unable to create a new tab.
  void StartTask(const optimization_guide::proto::BrowserAction& action,
                 StartTaskCallback callback);
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
                       StartTaskCallback callback);

  // Invokes the StartTask callback when initializing a new task failed (e.g.
  // error creating a new tab). Must be called to reset from the "initializing"
  // state.
  void PostTaskForStartInitializationFailed(
      ActorCoordinator::StartTaskCallback callback);

  // Creates a new tab to be used for performing a task.
  void CreateNewTab(StartTaskCallback callback);

  void OnNewTabCreated(StartTaskCallback callback,
                       content::WebContents* web_contents);

  void OnMayActOnTabResponse(
      base::WeakPtr<tabs::TabInterface> tab,
      const optimization_guide::proto::BrowserAction& action,
      const url::Origin& evaluated_origin,
      ActionResultCallback callback,
      bool may_act);

  base::WeakPtr<ActorCoordinator> GetWeakPtr();

  bool initializing_new_task_ = false;
  raw_ptr<Profile> profile_;
  base::WeakPtr<tabs::TabInterface> task_tab_;
  std::unique_ptr<NewTabWebContentsObserver> new_tab_web_contents_observer_;
  ToolController tool_controller_;

  SEQUENCE_CHECKER(sequence_checker_);
  base::WeakPtrFactory<ActorCoordinator> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_ACTOR_COORDINATOR_H_
