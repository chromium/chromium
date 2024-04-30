// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "base/containers/flat_set.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "content/public/browser/dedicated_worker_service.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"
#include "content/public/browser/shared_worker_service.h"
#include "third_party/blink/public/common/tokens/tokens.h"

class Profile;

namespace content {
class RenderProcessHost;
}

namespace url {
class Origin;
}

namespace task_manager {

class WorkerTask;
class WorkerTaskProvider;

// This is a helper class owned by WorkerTaskProvider to track all workers
// associated with a single profile. It manages the WorkerTasks and sends
// lifetime notifications to the WorkerTaskProvider.
class PerProfileWorkerTaskTracker
    : public content::DedicatedWorkerService::Observer,
      public content::SharedWorkerService::Observer,
      public content::ServiceWorkerContextObserver {
 public:
  PerProfileWorkerTaskTracker(WorkerTaskProvider* worker_task_provider,
                              Profile* profile);

  ~PerProfileWorkerTaskTracker() override;

  PerProfileWorkerTaskTracker(const PerProfileWorkerTaskTracker&) = delete;
  PerProfileWorkerTaskTracker& operator=(const PerProfileWorkerTaskTracker&) =
      delete;

  // content::DedicatedWorkerService::Observer:
  void OnWorkerCreated(const blink::DedicatedWorkerToken& worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       content::DedicatedWorkerCreator creator) override;
  void OnBeforeWorkerDestroyed(
      const blink::DedicatedWorkerToken& worker_token,
      content::DedicatedWorkerCreator creator) override;
  void OnFinalResponseURLDetermined(
      const blink::DedicatedWorkerToken& worker_token,
      const GURL& url) override;

  // content::SharedWorkerService::Observer:
  void OnWorkerCreated(const blink::SharedWorkerToken& shared_worker_token,
                       int worker_process_id,
                       const url::Origin& security_origin,
                       const base::UnguessableToken& dev_tools_token) override;
  void OnBeforeWorkerDestroyed(
      const blink::SharedWorkerToken& shared_worker_token) override;
  void OnFinalResponseURLDetermined(
      const blink::SharedWorkerToken& shared_worker_token,
      const GURL& url) override;
  void OnClientAdded(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId render_frame_host_id) override {}
  void OnClientRemoved(
      const blink::SharedWorkerToken& shared_worker_token,
      content::GlobalRenderFrameHostId render_frame_host_id) override {}

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(int64_t version_id) override;

 private:
  // Creates a WorkerTask and inserts it into |out_worker_tasks|. Then it
  // notifies the |worker_task_provider_| about the new Task.
  //
  // Note that this function is templated because each worker type uses a
  // different type as its ID.
  template <typename WorkerId>
  void CreateWorkerTask(
      const WorkerId& worker_id,
      Task::Type task_type,
      content::RenderProcessHost* worker_process_host,
      base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks);

  // Deletes an existing WorkerTask from |out_worker_tasks| and notifies
  // |worker_task_provider_| about the deletion of the task.
  //
  // Note that this function is templated because each worker type uses a
  // different type as its ID.
  template <typename WorkerId>
  void DeleteWorkerTask(
      const WorkerId& worker_id,
      base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks);

  // Sets the script URL of an existing WorkerTask.
  //
  // Note that this function is templated because each worker type uses a
  // different type as its ID.
  template <typename WorkerId>
  void SetWorkerTaskScriptUrl(
      const WorkerId& worker_id,
      const GURL& script_url,
      base::flat_map<WorkerId, std::unique_ptr<WorkerTask>>* out_worker_tasks);

  // The provider that gets notified when a WorkerTask is created/deleted.
  const raw_ptr<WorkerTaskProvider> worker_task_provider_;  // Owner.

  // For dedicated workers:
  base::ScopedObservation<content::DedicatedWorkerService,
                          content::DedicatedWorkerService::Observer>
      scoped_dedicated_worker_service_observation_{this};

  base::flat_map<blink::DedicatedWorkerToken, std::unique_ptr<WorkerTask>>
      dedicated_worker_tasks_;

  // For shared workers:
  base::ScopedObservation<content::SharedWorkerService,
                          content::SharedWorkerService::Observer>
      scoped_shared_worker_service_observation_{this};

  base::flat_map<blink::SharedWorkerToken, std::unique_ptr<WorkerTask>>
      shared_worker_tasks_;

  // For service workers:
  base::ScopedObservation<content::ServiceWorkerContext,
                          content::ServiceWorkerContextObserver>
      scoped_service_worker_context_observation_{this};

  base::flat_map<int64_t /*version_id*/, std::unique_ptr<WorkerTask>>
      service_worker_tasks_;

  // Because service worker notifications are asynchronous, it is possible to
  // be notified of the creation of a service worker after its render process
  // was deleted. Those workers are ignored.
  base::flat_set<int64_t /*version_id*/> ignored_service_worker_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_PER_PROFILE_WORKER_TASK_TRACKER_H_
