// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/process_snapshot_server.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"

namespace task_manager {

struct VmProcessData;

// This provides the task list for VMs. Currently this will only
// display tasks for the VMs themselves, later that will be expanded to also
// include the processes inside of the VM as part of the TaskGroup.
class VmProcessTaskProvider : public TaskProvider,
                              public ProcessSnapshotServer::Observer {
 public:
  VmProcessTaskProvider();
  ~VmProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // ProcessSnapshotServer::Observer:
  void OnProcessSnapshotRefreshed(
      const base::ProcessIterator::ProcessEntries& snapshot) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  void OnUpdateVmProcessList(const std::vector<VmProcessData>& vm_process_list);

  // The time at which the most recent process snapshot was received from the
  // ProcessSnapshotServer.
  base::Time last_process_snapshot_time_;

  // Map of PIDs to the corresponding Task object for a running VM.
  base::flat_map<base::ProcessId, std::unique_ptr<VmProcessTask>> task_map_;

  DISALLOW_COPY_AND_ASSIGN(VmProcessTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_
