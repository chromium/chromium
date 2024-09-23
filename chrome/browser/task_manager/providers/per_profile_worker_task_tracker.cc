// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/per_profile_worker_task_tracker.h"

#include "base/check.h"
#include "base/not_fatal_until.h"
#include "chrome/browser/task_manager/providers/worker_task.h"
#include "chrome/browser/task_manager/providers/worker_task_provider.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/storage_partition.h"

namespace task_manager {

PerProfileWorkerTaskTracker::PerProfileWorkerTaskTracker(
    WorkerTaskProvider* worker_task_provider,
    Profile* profile)
    : worker_task_provider_(worker_task_provider) {
  DCHECK(worker_task_provider);
  DCHECK(profile);

  content::StoragePartition* storage_partition =
      profile->GetDefaultStoragePartition();

  // Dedicated workers:
  content::DedicatedWorkerService* dedicated_worker_service =
      storage_partition->GetDedicatedWorkerService();
  scoped_dedicated_worker_service_observation_.Observe(
      dedicated_worker_service);
  dedicated_worker_service->EnumerateDedicatedWorkers(this);

  // Shared workers:
  content::SharedWorkerService* shared_worker_service =
      storage_partition->GetSharedWorkerService();
  scoped_shared_worker_service_observation_.Observe(shared_worker_service);
  shared_worker_service->EnumerateSharedWorkers(this);

  // Service workers:
  content::ServiceWorkerContext* service_worker_context =
      storage_partition->GetServiceWorkerContext();
  scoped_service_worker_context_observation_.Observe(service_worker_context);

  for (const auto& kv :
       service_worker_context->GetRunningServiceWorkerInfos()) {
    const int64_t version_id = kv.first;
    const content::ServiceWorkerRunningInfo& running_info = kv.second;
    OnVersionStartedRunning(version_id, running_info);
  }
}

PerProfileWorkerTaskTracker::~PerProfileWorkerTaskTracker() {
  // Notify the |worker_task_provider_| for all outstanding tasks that are about
  // to be deleted.
  for (const auto& kv : dedicated_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
  for (const auto& kv : shared_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
  for (const auto& kv : service_worker_tasks_)
    worker_task_provider_->OnWorkerTaskRemoved(kv.second.get());
}

void PerProfileWorkerTaskTracker::OnWorkerCreated(
    const blink::DedicatedWorkerToken& worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    content::DedicatedWorkerCreator creator) {
  auto* worker_process_host =
      content::RenderProcessHost::FromID(worker_process_id);
  DCHECK(worker_process_host);
  CreateWorkerTask(worker_token, Task::Type::DEDICATED_WORKER,
                   worker_process_host, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnBeforeWorkerDestroyed(
    const blink::DedicatedWorkerToken& worker_token,
    content::DedicatedWorkerCreator creator) {
  DeleteWorkerTask(worker_token, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnFinalResponseURLDetermined(
    const blink::DedicatedWorkerToken& worker_token,
    const GURL& url) {
  SetWorkerTaskScriptUrl(worker_token, url, &dedicated_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnWorkerCreated(
    const blink::SharedWorkerToken& shared_worker_token,
    int worker_process_id,
    const url::Origin& security_origin,
    const base::UnguessableToken& dev_tools_token) {
  auto* worker_process_host =
      content::RenderProcessHost::FromID(worker_process_id);
  DCHECK(worker_process_host);
  CreateWorkerTask(shared_worker_token, Task::Type::SHARED_WORKER,
                   worker_process_host, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnBeforeWorkerDestroyed(
    const blink::SharedWorkerToken& shared_worker_token) {
  DeleteWorkerTask(shared_worker_token, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnFinalResponseURLDetermined(
    const blink::SharedWorkerToken& shared_worker_token,
    const GURL& url) {
  SetWorkerTaskScriptUrl(shared_worker_token, url, &shared_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnVersionStartedRunning(
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  auto* worker_process_host =
      content::RenderProcessHost::FromID(running_info.render_process_id);

  // It's possible that the renderer is already gone since the notification for
  // a service worker comes asynchronously. Ignore this worker.
  if (!worker_process_host) {
    // A matching OnVersionStoppedRunning() call is still expected for this
    // service worker.
    const bool inserted = ignored_service_worker_.insert(version_id).second;
    DCHECK(inserted);
    return;
  }

  CreateWorkerTask(version_id, Task::Type::SERVICE_WORKER, worker_process_host,
                   &service_worker_tasks_);
  SetWorkerTaskScriptUrl(version_id, running_info.script_url,
                         &service_worker_tasks_);
}

void PerProfileWorkerTaskTracker::OnVersionStoppedRunning(int64_t version_id) {
  size_t removed = ignored_service_worker_.erase(version_id);
  if (removed) {
    // A task for this service worker was never created. Ignore the
    // notification.
    return;
  }

  DeleteWorkerTask(version_id, &service_worker_tasks_);
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::CreateWorkerTask(
    const WorkerId& worker_id,
    Task::Type task_type,
    content::RenderProcessHost* worker_process_host,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  DCHECK(worker_process_host);
  auto insertion_result = out_worker_tasks->emplace(
      worker_id,
      std::make_unique<WorkerTask>(worker_process_host->GetProcess().Handle(),
                                   task_type, worker_process_host->GetID()));
  DCHECK(insertion_result.second);
  worker_task_provider_->OnWorkerTaskAdded(
      insertion_result.first->second.get());
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::DeleteWorkerTask(
    const WorkerId& worker_id,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  auto it = out_worker_tasks->find(worker_id);
  CHECK(it != out_worker_tasks->end(), base::NotFatalUntil::M130);
  worker_task_provider_->OnWorkerTaskRemoved(it->second.get());
  out_worker_tasks->erase(it);
}

template <typename WorkerId>
void PerProfileWorkerTaskTracker::SetWorkerTaskScriptUrl(
    const WorkerId& worker_id,
    const GURL& script_url,
    base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks) {
  auto it = out_worker_tasks->find(worker_id);
  CHECK(it != out_worker_tasks->end(), base::NotFatalUntil::M130);
  it->second->SetScriptUrl(script_url);
}

}  // namespace task_manager
