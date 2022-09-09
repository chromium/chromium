// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_H_

#include "chrome/browser/task_manager/providers/task.h"

class GURL;

namespace task_manager {

// This class represents a task that corresponds to a dedicated worker, a shared
// worker or a service worker.
// See https://w3c.github.io/workers/ or https://w3c.github.io/ServiceWorker/
// for more details.
class WorkerTask : public Task {
 public:
  WorkerTask(base::ProcessHandle handle,
             Task::Type task_type,
             int render_process_id);
  ~WorkerTask() override;

  // Non-copyable.
  WorkerTask(const WorkerTask& other) = delete;
  WorkerTask& operator=(const WorkerTask& other) = delete;

  // task_manager::Task:
  Task::Type GetType() const override;
  int GetChildProcessUniqueID() const override;

  // Invoked when the final response URL of the worker script is determined.
  void SetScriptUrl(const GURL& script_url);

 private:
  // The type of this worker task. Can be one of DEDICATED_WORKER, SHARED_WORKER
  // or SERVICE_WORKER.
  const Task::Type task_type_;

  // The unique ID of the RenderProcessHost.
  const int render_process_id_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_WORKER_TASK_H_
