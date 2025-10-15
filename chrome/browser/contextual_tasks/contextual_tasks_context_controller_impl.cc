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

FeatureEligibility
ContextualTasksContextControllerImpl::GetFeatureEligibility() {
  return {base::FeatureList::IsEnabled(contextual_tasks::kContextualTasks),
          aim_eligibility_service_->IsAimEligible()};
}

ContextualTask ContextualTasksContextControllerImpl::CreateTask() {
  return service_->CreateTask();
}

void ContextualTasksContextControllerImpl::GetTaskById(
    const base::Uuid& task_id,
    base::OnceCallback<void(std::optional<ContextualTask>)> callback) const {
  service_->GetTaskById(task_id, std::move(callback));
}

void ContextualTasksContextControllerImpl::GetTasks(
    base::OnceCallback<void(std::vector<ContextualTask>)> callback) const {
  service_->GetTasks(std::move(callback));
}

void ContextualTasksContextControllerImpl::DeleteTask(
    const base::Uuid& task_id) {
  service_->DeleteTask(task_id);
}

void ContextualTasksContextControllerImpl::AddThreadToTask(
    const base::Uuid& task_id,
    const Thread& thread) {
  service_->AddThreadToTask(task_id, thread);
}

void ContextualTasksContextControllerImpl::RemoveThreadFromTask(
    const base::Uuid& task_id,
    ThreadType type,
    const std::string& server_id) {
  service_->RemoveThreadFromTask(task_id, type, server_id);
}

void ContextualTasksContextControllerImpl::UpdateThreadTurnId(
    const base::Uuid& task_id,
    ThreadType thread_type,
    const std::string& server_id,
    const std::string& conversation_turn_id) {
  service_->UpdateThreadTurnId(task_id, thread_type, server_id,
                               conversation_turn_id);
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

void ContextualTasksContextControllerImpl::AttachSessionIdToTask(
    const base::Uuid& task_id,
    SessionID session_id) {
  service_->AttachSessionIdToTask(task_id, session_id);
}

void ContextualTasksContextControllerImpl::DetachSessionIdFromTask(
    const base::Uuid& task_id,
    SessionID session_id) {
  service_->DetachSessionIdFromTask(task_id, session_id);
}

std::optional<ContextualTask>
ContextualTasksContextControllerImpl::GetMostRecentContextualTaskForSessionID(
    SessionID session_id) const {
  return service_->GetMostRecentContextualTaskForSessionID(session_id);
}

void ContextualTasksContextControllerImpl::AddObserver(Observer* observer) {
  service_->AddObserver(observer);
}

void ContextualTasksContextControllerImpl::RemoveObserver(Observer* observer) {
  service_->RemoveObserver(observer);
}

base::WeakPtr<syncer::DataTypeControllerDelegate>
ContextualTasksContextControllerImpl::GetAiThreadControllerDelegate() {
  return service_->GetAiThreadControllerDelegate();
}

}  // namespace contextual_tasks
