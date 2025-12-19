// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is a temporary stub implementation for Android to allow compilation.
// Once the actor code and its dependencies are made compatible with Android,
// this file will be deleted and the real implementation in
// glic_actor_task_manager.cc will be used for all platforms.

#include "chrome/browser/glic/actor/glic_actor_task_manager.h"

#include "base/functional/callback.h"

namespace glic {

GlicActorTaskManager::GlicActorTaskManager(Profile* profile) {}

GlicActorTaskManager::~GlicActorTaskManager() = default;

void GlicActorTaskManager::CreateTask(
    base::WeakPtr<actor::ActorTaskDelegate> delegate,
    actor::webui::mojom::TaskOptionsPtr options,
    mojom::WebClientHandler::CreateTaskCallback callback) {
  std::move(callback).Run(
      base::unexpected(mojom::CreateTaskErrorReason::kTaskSystemUnavailable));
}

void GlicActorTaskManager::PerformActions(
    const std::vector<uint8_t>& actions_proto,
    mojom::WebClientHandler::PerformActionsCallback callback) {
  std::move(callback).Run(
      base::unexpected(mojom::PerformActionsErrorReason::kMissingTaskId));
}

void GlicActorTaskManager::StopActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskStopReason stop_reason) {}

void GlicActorTaskManager::PauseActorTask(
    actor::TaskId task_id,
    mojom::ActorTaskPauseReason pause_reason,
    tabs::TabInterface::Handle tab_handle) {}

void GlicActorTaskManager::ResumeActorTask(
    actor::TaskId task_id,
    const mojom::GetTabContextOptions& context_options,
    glic::mojom::WebClientHandler::ResumeActorTaskCallback callback) {
  std::move(callback).Run(mojom::GetContextResultWithActionResultCode::New(
      mojom::GetContextResult::NewErrorReason("Not implemented"),
      std::nullopt));
}

void GlicActorTaskManager::InterruptActorTask(actor::TaskId task_id) {}

void GlicActorTaskManager::UninterruptActorTask(actor::TaskId task_id) {}

void GlicActorTaskManager::CreateActorTab(
    actor::TaskId task_id,
    bool open_in_background,
    const std::optional<int32_t>& initiator_tab_id,
    const std::optional<int32_t>& initiator_window_id,
    glic::mojom::WebClientHandler::CreateActorTabCallback callback) {
  std::move(callback).Run(nullptr);
}

void GlicActorTaskManager::MaybeShowDeactivationToastUi() {}

void GlicActorTaskManager::CancelTask() {}

bool GlicActorTaskManager::IsActuating() const {
  return false;
}

base::WeakPtr<GlicActorTaskManager> GlicActorTaskManager::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace glic
