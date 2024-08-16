// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_
#define CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_

#include <optional>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/vm/vm_process_task.h"
#include "chromeos/ash/components/dbus/vm_concierge/concierge_service.pb.h"
#include "chromeos/ash/components/process_snapshot/process_snapshot_server.h"

namespace task_manager {

struct VmProcessData;

// This provides the task list for VMs. Currently this will only
// display tasks for the VMs themselves, later that will be expanded to also
// include the processes inside of the VM as part of the TaskGroup.
class VmProcessTaskProvider : public TaskProvider,
                              public ash::ProcessSnapshotServer::Observer {
 public:
  VmProcessTaskProvider();
  VmProcessTaskProvider(const VmProcessTaskProvider&) = delete;
  VmProcessTaskProvider& operator=(const VmProcessTaskProvider&) = delete;
  ~VmProcessTaskProvider() override;

  // task_manager::TaskProvider:
  Task* GetTaskOfUrlRequest(int child_id, int route_id) override;

  // ash::ProcessSnapshotServer::Observer:
  void OnProcessSnapshotRefreshed(
      const base::ProcessIterator::ProcessEntries& snapshot) override;

 private:
  // task_manager::TaskProvider:
  void StartUpdating() override;
  void StopUpdating() override;

  void OnUpdateVmProcessList(const std::vector<VmProcessData>& vm_process_list);

  // Called as a response to a ListVms made to the concierge.
  void OnListVms(const base::ProcessIterator::ProcessEntries& snapshot,
                 std::optional<vm_tools::concierge::ListVmsResponse> response);

  // The time at which the most recent process snapshot was received from the
  // `ash::ProcessSnapshotServer`.
  base::Time last_process_snapshot_time_;

  // Map of PIDs to the corresponding Task object for a running VM.
  base::flat_map<base::ProcessId, std::unique_ptr<VmProcessTask>> task_map_;

  // Always keep this the last member of this class to make sure it's the
  // first thing to be destructed.
  base::WeakPtrFactory<VmProcessTaskProvider> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_PROVIDERS_VM_VM_PROCESS_TASK_PROVIDER_H_
