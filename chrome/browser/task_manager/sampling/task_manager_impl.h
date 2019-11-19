// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IMPL_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/lazy_instance.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/task_manager/providers/task_provider.h"
#include "chrome/browser/task_manager/providers/task_provider_observer.h"
#include "chrome/browser/task_manager/sampling/task_group.h"
#include "chrome/browser/task_manager/task_manager_interface.h"
#include "gpu/ipc/common/memory_stats.h"
#include "services/network/public/mojom/network_service.mojom.h"
#include "services/resource_coordinator/public/cpp/memory_instrumentation/global_memory_dump.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/task_manager/sampling/arc_shared_sampler.h"
#endif  // defined(OS_CHROMEOS)

namespace task_manager {

class SharedSampler;

// Identifies the initiator of a network request, by a (child_id,
// route_id) tuple.
// BytesTransferredKey supports hashing and may be used as an unordered_map key.
struct BytesTransferredKey {
  // The unique ID of the host of the child process requester.
  int child_id;

  // The ID of the IPC route for the URLRequest (this identifies the
  // RenderView or like-thing in the renderer that the request gets routed
  // to).
  int route_id;

  struct Hasher {
    size_t operator()(const BytesTransferredKey& key) const;
  };

  bool operator==(const BytesTransferredKey& other) const;
};

// This is the entry of the unordered map that tracks bytes transfered by task.
struct BytesTransferredParam {
  // The number of bytes read.
  int64_t byte_read_count = 0;

  // The number of bytes sent.
  int64_t byte_sent_count = 0;
};

using BytesTransferredMap = std::unordered_map<BytesTransferredKey,
                                               BytesTransferredParam,
                                               BytesTransferredKey::Hasher>;

// Defines a concrete implementation of the TaskManagerInterface.
class TaskManagerImpl : public TaskManagerInterface,
                        public TaskProviderObserver {
 public:
  ~TaskManagerImpl() override;

  static TaskManagerImpl* GetInstance();

  // task_manager::TaskManagerInterface:
  void ActivateTask(TaskId task_id) override;
  bool IsTaskKillable(TaskId task_id) override;
  void KillTask(TaskId task_id) override;
  double GetPlatformIndependentCPUUsage(TaskId task_id) const override;
  base::Time GetStartTime(TaskId task_id) const override;
  base::TimeDelta GetCpuTime(TaskId task_id) const override;
  int64_t GetMemoryFootprintUsage(TaskId task_id) const override;
  int64_t GetSwappedMemoryUsage(TaskId task_id) const override;
  int64_t GetGpuMemoryUsage(TaskId task_id,
                            bool* has_duplicates) const override;
  int GetIdleWakeupsPerSecond(TaskId task_id) const override;
  int GetHardFaultsPerSecond(TaskId task_id) const override;
  int GetNaClDebugStubPort(TaskId task_id) const override;
  void GetGDIHandles(TaskId task_id,
                     int64_t* current,
                     int64_t* peak) const override;
  void GetUSERHandles(TaskId task_id,
                      int64_t* current,
                      int64_t* peak) const override;
  int GetOpenFdCount(TaskId task_id) const override;
  bool IsTaskOnBackgroundedProcess(TaskId task_id) const override;
  const base::string16& GetTitle(TaskId task_id) const override;
  const std::string& GetTaskNameForRappor(TaskId task_id) const override;
  base::string16 GetProfileName(TaskId task_id) const override;
  const gfx::ImageSkia& GetIcon(TaskId task_id) const override;
  const base::ProcessHandle& GetProcessHandle(TaskId task_id) const override;
  const base::ProcessId& GetProcessId(TaskId task_id) const override;
  Task::Type GetType(TaskId task_id) const override;
  SessionID GetTabId(TaskId task_id) const override;
  int GetChildProcessUniqueId(TaskId task_id) const override;
  void GetTerminationStatus(TaskId task_id,
                            base::TerminationStatus* out_status,
                            int* out_error_code) const override;
  int64_t GetNetworkUsage(TaskId task_id) const override;
  int64_t GetCumulativeNetworkUsage(TaskId task_id) const override;
  int64_t GetProcessTotalNetworkUsage(TaskId task_id) const override;
  int64_t GetCumulativeProcessTotalNetworkUsage(TaskId task_id) const override;
  int64_t GetSqliteMemoryUsed(TaskId task_id) const override;
  bool GetV8Memory(TaskId task_id,
                   int64_t* allocated,
                   int64_t* used) const override;
  bool GetWebCacheStats(TaskId task_id,
                        blink::WebCacheResourceTypeStats* stats) const override;
  int GetKeepaliveCount(TaskId task_id) const override;
  const TaskIdList& GetTaskIdsList() const override;
  TaskIdList GetIdsOfTasksSharingSameProcess(TaskId task_id) const override;
  size_t GetNumberOfTasksOnSameProcess(TaskId task_id) const override;
  bool IsRunningInVM(TaskId task_id) const override;
  TaskId GetTaskIdForWebContents(
      content::WebContents* web_contents) const override;

  // task_manager::TaskProviderObserver:
  void TaskAdded(Task* task) override;
  void TaskRemoved(Task* task) override;
  void TaskUnresponsive(Task* task) override;

  // Used when Network Service is enabled.
  // Receives total network usages from |NetworkService|.
  void OnTotalNetworkUsages(
      std::vector<network::mojom::NetworkUsagePtr> total_network_usages);

 private:
  using PidToTaskGroupMap =
      std::map<base::ProcessId, std::unique_ptr<TaskGroup>>;

  friend struct base::LazyInstanceTraitsBase<TaskManagerImpl>;

  TaskManagerImpl();

  void OnVideoMemoryUsageStatsUpdate(
      const gpu::VideoMemoryUsageStats& gpu_memory_stats);
  void OnReceivedMemoryDump(
      bool success,
      std::unique_ptr<memory_instrumentation::GlobalMemoryDump> dump);

  // task_manager::TaskManagerInterface:
  void Refresh() override;
  void StartUpdating() override;
  void StopUpdating() override;

  // Lookup a task by child_id and possibly route_id.
  Task* GetTaskByRoute(int child_id, int route_id) const;

  // Based on |param| the appropriate task will be updated by its network usage.
  // Returns true if it was able to match |param| to an existing task, returns
  // false otherwise, at which point the caller must explicitly match these
  // bytes to the browser process by calling this method again with
  // |param.origin_pid = 0| and |param.child_id = param.route_id = -1|.
  bool UpdateTasksWithBytesTransferred(const BytesTransferredKey& key,
                                       const BytesTransferredParam& param);

  PidToTaskGroupMap* GetVmPidToTaskGroupMap(Task::Type type);
  TaskGroup* GetTaskGroupByTaskId(TaskId task_id) const;
  Task* GetTaskByTaskId(TaskId task_id) const;

  // Called back by a TaskGroup when the resource calculations done on the
  // background thread has completed.
  void OnTaskGroupBackgroundCalculationsDone();

  const base::Closure on_background_data_ready_callback_;

  // Map TaskGroups by the IDs of the processes they represent.
  PidToTaskGroupMap task_groups_by_proc_id_;

  // Map ARC VM PidToTaskGroupMaps by the task type. This should be separate
  // from the non-VM map |task_groups_by_proc_id_| as there can be conflicting
  // PIDs.
  PidToTaskGroupMap arc_vm_task_groups_by_proc_id_;

  // Map each task by its ID to the TaskGroup on which it resides.
  // Keys are unique but values will have duplicates (i.e. multiple tasks
  // running on the same process represented by a single TaskGroup).
  std::map<TaskId, TaskGroup*> task_groups_by_task_id_;

  // A cached sorted list of the task IDs.
  mutable std::vector<TaskId> sorted_task_ids_;

  // Used when Network Service is enabled.
  // Stores the total network usages per |process_id, routing_id| from last
  // refresh.
  BytesTransferredMap last_refresh_total_network_usages_map_;

  // The list of the task providers that are owned and observed by this task
  // manager implementation.
  std::vector<std::unique_ptr<TaskProvider>> task_providers_;

  // The current GPU memory usage stats that was last received from the
  // GpuDataManager.
  gpu::VideoMemoryUsageStats gpu_memory_stats_;

  // The specific blocking pool SequencedTaskRunner that will be used to make
  // sure TaskGroupSampler posts their refreshes serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  // A special sampler shared with all instances of TaskGroup that calculates a
  // subset of resources for all processes at once.
  scoped_refptr<SharedSampler> shared_sampler_;

#if defined(OS_CHROMEOS)
  // A sampler shared with all instances of TaskGroup that hold ARC tasks and
  // calculates memory footprint for all processes at once.
  std::unique_ptr<ArcSharedSampler> arc_shared_sampler_;
#endif  // defined(OS_CHROMEOS)

  // This will be set to true while there are observers and the task manager is
  // running.
  bool is_running_;

  // This is set to true while waiting for a global memory dump from
  // memory_instrumentation.
  bool waiting_for_memory_dump_;

  base::WeakPtrFactory<TaskManagerImpl> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(TaskManagerImpl);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_MANAGER_IMPL_H_
