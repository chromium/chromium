// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
#define CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_

#include "base/functional/callback_forward.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/task_id.h"
#include "chrome/browser/glic/host/context/glic_tab_data.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/actor.mojom-forward.h"
#include "components/optimization_guide/proto/features/actions_data.pb.h"

namespace actor {
class ActorCoordinator;
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

  // Invokes the actor to complete an action.
  void Act(FocusedTabData focused_tab_data,
           const optimization_guide::proto::BrowserAction& action,
           const mojom::GetTabContextOptions& options,
           mojom::WebClientHandler::ActInFocusedTabCallback callback);

  void StopTask(actor::TaskId task_id);
  void PauseTask(actor::TaskId task_id);
  void ResumeTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);

  bool IsActorCoordinatorActingOnTab(const content::WebContents* tab) const;

  actor::ActorCoordinator& GetActorCoordinatorForTesting();

 private:
  // Handles a new task being started, and then performs the action that
  // initiated the task.
  void OnTaskStarted(const optimization_guide::proto::BrowserAction& action,
                     const mojom::GetTabContextOptions& options,
                     mojom::WebClientHandler::ActInFocusedTabCallback callback,
                     base::WeakPtr<tabs::TabInterface> tab) const;

  // Core logic to execute an action.
  void ActImpl(FocusedTabData focused_tab_data,
               const optimization_guide::proto::BrowserAction& action,
               const mojom::GetTabContextOptions& options,
               mojom::WebClientHandler::ActInFocusedTabCallback callback) const;

  // Handles the result of the action, returning new page context if necessary.
  void OnActionFinished(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      mojom::WebClientHandler::ActInFocusedTabCallback callback,
      actor::mojom::ActionResultPtr result) const;

  void GetContextFromFocusedTab(
      FocusedTabData focused_tab_data,
      const mojom::GetTabContextOptions& options,
      mojom::WebClientHandler::GetContextFromFocusedTabCallback callback) const;

  actor::ActorCoordinator* GetActorCoordinator() const;

  base::WeakPtr<const GlicActorController> GetWeakPtr() const;
  base::WeakPtr<GlicActorController> GetWeakPtr();

  raw_ptr<Profile> profile_;
  // The most recently created task, or nullptr if no task has ever been
  // created.
  raw_ptr<actor::ActorTask> actor_task_ = nullptr;
  base::WeakPtrFactory<GlicActorController> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_HOST_GLIC_ACTOR_CONTROLLER_H_
