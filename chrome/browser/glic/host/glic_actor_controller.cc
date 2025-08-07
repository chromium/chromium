// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_actor_controller.h"

#include <algorithm>
#include <cstddef>
#include <memory>

#include "base/location.h"
#include "base/types/cxx23_to_underlying.h"
#include "chrome/browser/actor/actor_keyed_service.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/aggregated_journal.h"
#include "chrome/browser/actor/execution_engine.h"
#include "chrome/browser/glic/host/context/glic_page_context_fetcher.h"
#include "chrome/browser/glic/host/context/glic_sharing_manager_impl.h"
#include "chrome/browser/glic/host/glic.mojom.h"
#include "chrome/common/actor.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace glic {

namespace {

mojom::GetTabContextOptionsPtr ActionableOptions(
    const mojom::GetTabContextOptions& options) {
  // TODO(khushalsagar): Ideally this should be set by the web UI instead of
  // overriding here for actor mode.
  auto actionable_context_options = options.Clone();
  actionable_context_options->annotated_page_content_mode = optimization_guide::
      proto::ANNOTATED_PAGE_CONTENT_MODE_ACTIONABLE_ELEMENTS;
  return actionable_context_options;
}

}  // namespace

GlicActorController::GlicActorController(Profile* profile) : profile_(profile) {
  CHECK(profile_);
  actor::ExecutionEngine::RegisterWithProfile(profile_);
}

GlicActorController::~GlicActorController() = default;

// TODO(mcnee): Determine if we need additional mechanisms, within the browser,
// to stop a task.
void GlicActorController::StopTask(actor::TaskId task_id,
                                   mojom::ActorTaskStopReason stop_reason) {
  actor::ActorTask* task = GetCurrentTask();
  if (!task) {
    return;
  }
  actor::ActorKeyedService::Get(profile_.get())
      ->StopTask(task->id(), /*success=*/true);
}

void GlicActorController::PauseTask(actor::TaskId task_id,
                                    mojom::ActorTaskPauseReason pause_reason) {
  actor::ActorTask* task = GetCurrentTask();
  if (!task) {
    return;
  }
  switch (pause_reason) {
    case mojom::ActorTaskPauseReason::kPausedByModel:
      task->Pause(/*from_actor=*/true);
      break;
    case mojom::ActorTaskPauseReason::kPausedByUser:
      task->Pause(/*from_actor=*/false);
      break;
  }
}

void GlicActorController::ResumeTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  // TODO(https://crbug.com/433328453): Add stability metrics specific to actor
  // tasks.
  actor::ActorTask* task = GetCurrentTask();
  if (!task || !task->IsPaused()) {
    std::move(callback).Run(mojom::GetContextResult::NewErrorReason(
        std::string("task does not exist or was not paused")));
    return;
  }
  task->Resume();
  tabs::TabInterface* tab_of_resumed_task = task->GetTabForObservation();
  if (!tab_of_resumed_task) {
    std::move(callback).Run(glic::mojom::GetContextResult::NewErrorReason(
        std::string("tab does not exist")));
    return;
  }

  auto fetcher_callback = base::BindOnce(
      [](glic::mojom::WebClientHandler::ResumeActorTaskCallback final_callback,
         base::expected<glic::mojom::GetContextResultPtr,
                        page_content_annotations::FetchPageContextErrorDetails>
             result) {
        if (!result.has_value()) {
          std::move(final_callback)
              .Run(glic::mojom::GetContextResult::NewErrorReason(
                  result.error().message));
        } else {
          std::move(final_callback).Run(std::move(result.value()));
        }
      },
      std::move(callback));

  glic::FetchPageContext(tab_of_resumed_task,
                         *ActionableOptions(context_options),
                         std::move(fetcher_callback));
}

actor::ActorTask* GlicActorController::GetCurrentTask() const {
  return actor::ActorKeyedService::Get(profile_)->GetMostRecentTask();
}

}  // namespace glic
