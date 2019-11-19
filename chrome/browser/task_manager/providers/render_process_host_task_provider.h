// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_

#include <map>
#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_service.h"

namespace task_manager {

class ChildProcessTask;

// This provides tasks that represent RenderProcessHost processes. It does so by
// listening to the notification service for the creation and destruction of the
// RenderProcessHost.
class RenderProcessHostTaskProvider : public TaskProvider,
                                      public content::NotificationObserver {
 public:
  RenderProcessHostTaskProvider();
  ~RenderProcessHostTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // content::NotificationObserver:
  void Observe(int type,
               const content::NotificationSource& source,
               const content::NotificationDetails& details) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates a RenderProcessHostTask from the given |data| and notifies the
  // observer of its addition.
  void CreateTask(const int render_process_host_id);

  // Deletes a RenderProcessHostTask whose |render_process_host_id| is provided
  // after notifying the observer of its deletion.
  void DeleteTask(const int render_process_host_id);

  std::map<int, std::unique_ptr<ChildProcessTask>> tasks_by_rph_id_;

  // Object for registering notification requests.
  content::NotificationRegistrar registrar_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<RenderProcessHostTaskProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(RenderProcessHostTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_RENDER_PROCESS_HOST_TASK_PROVIDER_H_
