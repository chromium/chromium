// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_controller_impl.h"

#include <optional>
#include <vector>

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/single_thread_task_runner.h"
#include "components/contextual_tasks/public/contextual_tasks_service.h"
#include "components/contextual_tasks/public/features.h"
#include "components/omnibox/browser/aim_eligibility_service.h"
#include "components/sessions/core/session_id.h"

namespace contextual_tasks {

ContextualTasksContextControllerImpl::ContextualTasksContextControllerImpl(
    ContextualTasksService* service,
    AimEligibilityService* aim_eligibility_service)
    : service_(service), aim_eligibility_service_(aim_eligibility_service) {}

ContextualTasksContextControllerImpl::~ContextualTasksContextControllerImpl() =
    default;

ContextualTask ContextualTasksContextControllerImpl::CreateTask() {
  return service_->CreateTask();
}

void ContextualTasksContextControllerImpl::AssignThreadToTask(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    const std::string& conversation_turn_id,
    std::optional<std::string> title) {
  service_->AddThreadToTask(
      task_id,
      Thread(thread_type, server_id, title.value_or(""), conversation_turn_id));
}

void ContextualTasksContextControllerImpl::UpdateThreadTurnId(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    const std::string& conversation_turn_id) {
  service_->UpdateThreadTurnId(task_id, thread_type, server_id,
                               conversation_turn_id);
}

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

void ContextualTasksContextControllerImpl::AttachUrlToTask(
    const base::Uuid& task_id,
    const GURL& url) {
  service_->AttachUrlToTask(task_id, url);
}

void ContextualTasksContextControllerImpl::DetachUrlFromTask(
    const base::Uuid& task_id,
    const GURL& url) {
  service_->DetachUrlFromTask(task_id, url);
}

void ContextualTasksContextControllerImpl::GetContextForTask(
    const base::Uuid& task_id,
    const std::set<ContextualTaskContextSource>& sources,
    base::OnceCallback<void(std::unique_ptr<ContextualTaskContext>)>
        context_callback) {
  service_->GetContextForTask(task_id, sources, std::move(context_callback));
}

FeatureEligibility
ContextualTasksContextControllerImpl::GetFeatureEligibility() {
  return {base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks),
          aim_eligibility_service_->IsAimEligible()};
}

}  // namespace contextual_tasks
