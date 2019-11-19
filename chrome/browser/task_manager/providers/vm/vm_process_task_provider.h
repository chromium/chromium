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
#include "base/timer/timer.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"

namespace task_manager {

struct VmProcessData;

// This provides the task list for VMs. Currently this will only
// display tasks for the VMs themselves, later that will be expanded to also
// include the processes inside of the VM as part of the TaskGroup.
class VmProcessTaskProvider : public TaskProvider {
 public:
  VmProcessTaskProvider();
  ~VmProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  void RequestVmProcessList();
  void OnUpdateVmProcessList(const std::vector<VmProcessData>& vm_process_list);

  // Map of PIDs to the corresponding Task object for a running VM.
  base::flat_map<base::ProcessId, std::unique_ptr<VmProcessTask>> task_map_;

  // There are some expensive tasks such as traverse whole process tree that
  // we can't do it on the UI thread.
  scoped_refptr<base::SequencedTaskRunner> task_runner_;

  // For refreshing our process list while we are active.
  base::RepeatingTimer refresh_timer_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<VmProcessTaskProvider> weak_ptr_factory_{this};

  DISALLOW_COPY_AND_ASSIGN(VmProcessTaskProvider);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_
