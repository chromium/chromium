// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <utility>

#include "base/scoped_observer.h"
#include "chrome/browser/task_manager/providers/service_worker_task.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/service_worker_context.h"
#include "content/public/browser/service_worker_context_observer.h"

namespace task_manager {

// This provides tasks that describe running service workers
// (https://w3c.github.io/ServiceWorker/). It adds itself as an observer of
// ServiceWorkerContext to listen to the running status changes of the service
// workers for the creation and destruction of the tasks.
class ServiceWorkerTaskProvider : public TaskProvider,
                                  public content::NotificationObserver,
                                  public content::ServiceWorkerContextObserver {
 public:
  ServiceWorkerTaskProvider();
  ~ServiceWorkerTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

  // content::ServiceWorkerContextObserver:
  void OnVersionStartedRunning(
      content::ServiceWorkerContext* context,
      int64_t version_id,
      const content::ServiceWorkerRunningInfo& running_info) override;
  void OnVersionStoppedRunning(content::ServiceWorkerContext* context,
                               int64_t version_id) override;
  void OnDestruct(content::ServiceWorkerContext* context) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates ServiceWorkerTasks for the given |profile| and notifies the
  // observer of their additions.
  void CreateTasksForProfile(Profile* profile);

  // Creates a ServiceWorkerTask from the given |running_info| and notifies the
  // observer of its addition.
  void CreateTask(content::ServiceWorkerContext* context,
                  int64_t version_id,
                  const content::ServiceWorkerRunningInfo& running_info);

  // Deletes a ServiceWorkerTask with the |version_id| after notifying the
  // observer of its deletion.
  void DeleteTask(content::ServiceWorkerContext* context, int version_id);

  // Called after a profile has been created.
  void OnProfileCreated(Profile* profile);

  using ServiceWorkerTaskKey =
      std::pair<content::ServiceWorkerContext*, int64_t /*version_id*/>;
  using ServiceWorkerTaskMap =
      std::map<ServiceWorkerTaskKey, std::unique_ptr<ServiceWorkerTask>>;
  ServiceWorkerTaskMap service_worker_task_map_;

  content::NotificationRegistrar registrar_;

  ScopedObserver<content::ServiceWorkerContext,
                 content::ServiceWorkerContextObserver>
      scoped_context_observer_{this};

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_PROVIDER_H_
