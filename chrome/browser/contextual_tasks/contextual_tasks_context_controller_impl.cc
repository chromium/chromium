// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include <optional>
#include <vector>

#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

ContextualTasksContextControllerImpl::ContextualTasksContextControllerImpl(
    ContextualTasksService* service)
    : service_(service) {}

ContextualTasksContextControllerImpl::~ContextualTasksContextControllerImpl() =
    default;

void ContextualTasksContextControllerImpl::GetTasks(
    base::OnceCallback<void(std::vector<ContextualTask>)> callback) {
  service_->GetTasks(std::move(callback));
}

void ContextualTasksContextControllerImpl::GetTask(
    const base::Uuid& task_id,
    base::OnceCallback<void(std::optional<ContextualTask>)> callback) {
  service_->GetTaskById(task_id, std::move(callback));
}

void ContextualTasksContextControllerImpl::AssociateTabWithTask(
    SessionID tab_session_id,
    const base::Uuid& task_id) {
  service_->AttachSessionIdToTask(task_id, tab_session_id);
}

void ContextualTasksContextControllerImpl::GetSelectedTaskForTab(
    SessionID tab_session_id,
    base::OnceCallback<void(std::optional<ContextualTask>)>
        selected_task_callback) {
  std::move(selected_task_callback)
      .Run(service_->GetMostRecentContextualTaskForSessionID(tab_session_id));
}

}  // namespace contextual_tasks
