// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_

#include <memory>

#include "base/containers/flat_map.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "content/public/browser/browser_child_process_observer.h"
#include "content/public/common/child_process_id.h"

namespace content {
struct ChildProcessData;
}

namespace task_manager {

class ChildProcessTask;

// Defines a provider to provide the tasks that represent various types of child
// processes such as the GPU process or a plugin process ... etc.
class ChildProcessTaskProvider
    : public TaskProvider,
      public content::BrowserChildProcessObserver {
 public:
  ChildProcessTaskProvider();
  ChildProcessTaskProvider(const ChildProcessTaskProvider&) = delete;
  ChildProcessTaskProvider& operator=(const ChildProcessTaskProvider&) = delete;
  ~ChildProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // content::BrowserChildProcessObserver:
  void BrowserChildProcessLaunchedAndConnected(
      const content::ChildProcessData& data,
      const base::Process& process) override;
  void BrowserChildProcessHostDisconnected(
      const content::ChildProcessData& data) override;

 private:
  friend class ChildProcessTaskTest;

  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  // Creates a ChildProcessTask from the given |data| and notifies the observer
  // of its addition.
  void CreateTask(const content::ChildProcessData& data,
                  const base::Process& process);

  // Deletes a ChildProcessTask whose `child_process_id` is provided after
  // notifying the observer of its deletion.
  void DeleteTask(content::ChildProcessId child_process_id);

  // A map to track ChildProcessTasks by their unique child process id.
  base::flat_map<content::ChildProcessId, std::unique_ptr<ChildProcessTask>>
      tasks_by_child_id_;
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_CHILD_PROCESS_TASK_PROVIDER_H_
