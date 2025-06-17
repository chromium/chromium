// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"
#include "components/optimization_guide/proto/features/model_prototyping.pb.h"

namespace optimization_guide::proto {
class BrowserStartTaskResult;
}

namespace actor {
class ExecutionEngine;
class ActorTask;
}  // namespace actor

namespace content {
class WebContents;
}  // namespace content

namespace tabs {
class TabInterface;
}  // namespace tabs

namespace glic {

// Controls the interaction with the actor to complete an action.
class GlicActorController {
 public:
  explicit GlicActorController(Profile* profile);
  GlicActorController(const GlicActorController&) = delete;
  GlicActorController& operator=(const GlicActorController&) = delete;
  ~GlicActorController();

  // ActorKeyedService, the underlying framework, supports multi-tab actuation.
  // But this class does not because it does not expose the concept of
  // start/stop task. Instead it keeps track of any ongoing task, and implicitly
  // creates one for Act() if one does not already exist.
  // Invokes the actor to complete an action.
  void Act(const optimization_guide::proto::BrowserAction& action,
           const mojom::GetTabContextOptions& options,
           mojom::WebClientHandler::ActInFocusedTabCallback callback);

  void StopTask(actor::TaskId task_id);
  void PauseTask(actor::TaskId task_id);
  void ResumeTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);

  // These may not be necessarily generate actor tasks, but they are
  // useful for recording in the ActorJournal.
  void OnUserInputSubmitted();
  void OnRequestStarted();
  void OnResponseStarted();
  void OnResponseStopped();

  bool IsExecutionEngineActingOnTab(const content::WebContents* tab) const;

  actor::ExecutionEngine& GetExecutionEngineForTesting(tabs::TabInterface* tab);

 private:
  void OnTaskStartedForAct(
      const optimization_guide::proto::BrowserAction& action,
      const mojom::GetTabContextOptions& options,
      mojom::WebClientHandler::ActInFocusedTabCallback callback,
      optimization_guide::proto::BrowserStartTaskResult result);

  // Core logic to execute an action.
  void ActImpl(const optimization_guide::proto::BrowserAction& action,
               const mojom::GetTabContextOptions& options,
               mojom::WebClientHandler::ActInFocusedTabCallback callback) const;

  // Handles the result of the action, returning new page context if necessary.
  void OnActionFinished(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& options,
      mojom::WebClientHandler::ActInFocusedTabCallback callback,
      actor::mojom::ActionResultPtr result) const;

  actor::ExecutionEngine* GetExecutionEngine() const;

  base::WeakPtr<const GlicActorController> GetWeakPtr() const;
  base::WeakPtr<GlicActorController> GetWeakPtr();

  class OngoingRequest;

  raw_ptr<Profile> profile_;
  // The most recently created task, or nullptr if no task has ever been
  // created.
  raw_ptr<actor::ActorTask> actor_task_ = nullptr;
  // True if and only if a task is in the process of being started.
  bool starting_task_ = false;
  std::unique_ptr<OngoingRequest> current_request_;
  base::WeakPtrFactory<GlicActorController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
