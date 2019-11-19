// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_H_

#include "chrome/browser/task_manager/providers/task.h"
#include "url/gurl.h"

namespace task_manager {

// This class represents a task that corresponds to a service worker.
// https://w3c.github.io/ServiceWorker/
class ServiceWorkerTask : public Task {
 public:
  ServiceWorkerTask(base::ProcessHandle handle,
                    int render_process_id,
                    const GURL& script_url);
  ~ServiceWorkerTask() override;

  // task_manager::Task implementation:
  Task::Type GetType() const override;
  int GetChildProcessUniqueID() const override;

 private:
  // The unique ID of the RenderProcessHost.
  const int render_process_id_;

  DISALLOW_COPY_AND_ASSIGN(ServiceWorkerTask);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_SERVICE_WORKER_TASK_H_
