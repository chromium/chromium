// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/providers/service_worker_task_provider.h"

#include "base/stl_util.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/service_worker_running_info.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/child_process_host.h"
#include "third_party/blink/public/mojom/service_worker/service_worker_object.mojom.h"

using content::BrowserThread;

namespace task_manager {

ServiceWorkerTaskProvider::ServiceWorkerTaskProvider() = default;
ServiceWorkerTaskProvider::~ServiceWorkerTaskProvider() = default;

Task* ServiceWorkerTaskProvider::GetTaskOfUrlRequest(int child_id,
                                                     int route_id) {
  return nullptr;
}

void ServiceWorkerTaskProvider::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  switch (type) {
    case chrome::NOTIFICATION_PROFILE_CREATED: {
      OnProfileCreated(content::Source<Profile>(source).ptr());
      break;
    }
    default:
      NOTREACHED() << type;
  }
}

void ServiceWorkerTaskProvider::OnVersionStartedRunning(
    content::ServiceWorkerContext* context,
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  CreateTask(context, version_id, running_info);
}
void ServiceWorkerTaskProvider::OnVersionStoppedRunning(
    content::ServiceWorkerContext* context,
    int64_t version_id) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  DeleteTask(context, version_id);
}

void ServiceWorkerTaskProvider::OnDestruct(
    content::ServiceWorkerContext* context) {
  scoped_context_observer_.Remove(context);
}

void ServiceWorkerTaskProvider::StartUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);
  registrar_.Add(this, chrome::NOTIFICATION_PROFILE_CREATED,
                 content::NotificationService::AllBrowserContextsAndSources());

  ProfileManager* profile_manager = g_browser_process->profile_manager();
  if (profile_manager) {
    auto loaded_profiles = profile_manager->GetLoadedProfiles();
    for (auto* profile : loaded_profiles) {
      CreateTasksForProfile(profile);

      // If the incognito window is open, we have to check its profile and
      // create the tasks if there are any.
      if (profile->HasOffTheRecordProfile())
        CreateTasksForProfile(profile->GetOffTheRecordProfile());
    }
  }
}

void ServiceWorkerTaskProvider::StopUpdating() {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  // Stop listening to NOTIFICATION_PROFILE_CREATED.
  registrar_.RemoveAll();

  // Stop observing contexts.
  scoped_context_observer_.RemoveAll();

  // Delete all tracked tasks.
  service_worker_task_map_.clear();
}

void ServiceWorkerTaskProvider::CreateTasksForProfile(Profile* profile) {
  content::ServiceWorkerContext* context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetServiceWorkerContext();

  if (!scoped_context_observer_.IsObserving(context))
    scoped_context_observer_.Add(context);

  for (const auto& kv : context->GetRunningServiceWorkerInfos()) {
    const int64_t version_id = kv.first;
    const content::ServiceWorkerRunningInfo& running_info = kv.second;

    CreateTask(context, version_id, running_info);
  }
}

void ServiceWorkerTaskProvider::CreateTask(
    content::ServiceWorkerContext* context,
    int64_t version_id,
    const content::ServiceWorkerRunningInfo& running_info) {
  DCHECK_CURRENTLY_ON(BrowserThread::UI);

  const ServiceWorkerTaskKey key(context, version_id);
  DCHECK(!base::Contains(service_worker_task_map_, key));

  const int render_process_id = running_info.render_process_id;
  auto* host = content::RenderProcessHost::FromID(render_process_id);
  auto result = service_worker_task_map_.emplace(
      key, std::make_unique<ServiceWorkerTask>(host->GetProcess().Handle(),
                                               render_process_id,
                                               running_info.script_url));

  DCHECK(result.second);
  NotifyObserverTaskAdded(result.first->second.get());
}

void ServiceWorkerTaskProvider::DeleteTask(
    content::ServiceWorkerContext* context,
    int version_id) {
  const ServiceWorkerTaskKey key(context, version_id);
  auto it = service_worker_task_map_.find(key);
  DCHECK(it != service_worker_task_map_.end());

  NotifyObserverTaskRemoved(it->second.get());
  service_worker_task_map_.erase(it);
}

void ServiceWorkerTaskProvider::OnProfileCreated(Profile* profile) {
  content::ServiceWorkerContext* context =
      content::BrowserContext::GetDefaultStoragePartition(profile)
          ->GetServiceWorkerContext();
  scoped_context_observer_.Add(context);
}

}  // namespace task_manager
