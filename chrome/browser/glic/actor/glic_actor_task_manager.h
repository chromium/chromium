// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_
#define CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom-forward.h"
#include "chrome/common/actor/task_id.h"
#include "chrome/common/actor_webui.mojom.h"
#include "components/tabs/public/tab_interface.h"

class Profile;

namespace actor {
struct ActionResultWithLatencyInfo;
class ActorKeyedService;
class ActorTaskDelegate;
class ActorTask;
}  // namespace actor

namespace glic {

// Manages actor-related tasks for GlicKeyedService.
class GlicActorTaskManager {
 public:
  GlicActorTaskManager(Profile* profile,
                       actor::ActorKeyedService* actor_keyed_service);
  GlicActorTaskManager(const GlicActorTaskManager&) = delete;
  GlicActorTaskManager& operator=(const GlicActorTaskManager&) = delete;
  ~GlicActorTaskManager();

  void CreateTask(base::WeakPtr<actor::ActorTaskDelegate> delegate,
                  actor::webui::mojom::TaskOptionsPtr options,
                  mojom::WebClientHandler::CreateTaskCallback callback);
  void PerformActions(const std::vector<uint8_t>& actions_proto,
                      mojom::WebClientHandler::PerformActionsCallback callback);
  void StopActorTask(actor::TaskId task_id,
                     mojom::ActorTaskStopReason stop_reason);
  void PauseActorTask(actor::TaskId task_id,
                      mojom::ActorTaskPauseReason pause_reason,
                      tabs::TabInterface::Handle tab_handle);
  void ResumeActorTask(
      actor::TaskId task_id,
      const mojom::GetTabContextOptions& context_options,
      glic::mojom::WebClientHandler::ResumeActorTaskCallback callback);
  void InterruptActorTask(actor::TaskId task_id);
  void UninterruptActorTask(actor::TaskId task_id);
  void CreateActorTab(
      actor::TaskId task_id,
      bool open_in_background,
      const std::optional<int32_t>& initiator_tab_id,
      const std::optional<int32_t>& initiator_window_id,
      glic::mojom::WebClientHandler::CreateActorTabCallback callback);
  void MaybeShowDeactivationToastUi();

  void CancelTask();
  bool IsActuating() const;

  base::WeakPtr<GlicActorTaskManager> GetWeakPtr();

 private:
  void PerformActionsFinished(
      mojom::WebClientHandler::PerformActionsCallback callback,
      actor::TaskId task_id,
      base::TimeTicks start_time,
      bool skip_async_observation_information,
      actor::mojom::ActionResultCode result_code,
      std::optional<size_t> index_of_failed_action,
      std::vector<actor::ActionResultWithLatencyInfo> action_results);
  void ReloadTab(actor::ActorTask& task, base::OnceClosure callback);
  void CreateActorTabFinished(
      glic::mojom::WebClientHandler::CreateActorTabCallback callback,
      tabs::TabInterface* new_tab);
  void ReloadObserverDone(tabs::TabHandle tab_handle,
                          base::OnceClosure callback,
                          actor::ObservationDelayController::Result result);
  void ResetTaskState();

  raw_ptr<Profile> profile_;
  raw_ptr<actor::ActorKeyedService> actor_keyed_service_;

  actor::TaskId current_task_id_;
  bool attempted_reload_ = false;
  std::unique_ptr<actor::ObservationDelayController> reload_observer_;

  base::WeakPtrFactory<GlicActorTaskManager> weak_ptr_factory_{this};
};

}  // namespace glic

#endif  // CHROME_BROWSER_GLIC_ACTOR_GLIC_ACTOR_TASK_MANAGER_H_
