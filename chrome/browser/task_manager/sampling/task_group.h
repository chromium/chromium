// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <optional>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/process/process_handle.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/task_manager/providers/task.h"
#include "chrome/browser/task_manager/sampling/shared_sampler.h"
#include "chrome/browser/task_manager/sampling/task_group_sampler.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "components/nacl/common/buildflags.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/task_manager/sampling/arc_shared_sampler.h"
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

namespace gpu {
struct VideoMemoryUsageStats;
}

namespace task_manager {

class CrosapiTaskProviderAsh;

// A mask for refresh flags that are not supported by VM tasks.
constexpr int kUnsupportedVMRefreshFlags =
    REFRESH_TYPE_CPU | REFRESH_TYPE_SWAPPED_MEM | REFRESH_TYPE_GPU_MEMORY |
    REFRESH_TYPE_V8_MEMORY | REFRESH_TYPE_SQLITE_MEMORY |
    REFRESH_TYPE_WEBCACHE_STATS | REFRESH_TYPE_NETWORK_USAGE |
    REFRESH_TYPE_NACL | REFRESH_TYPE_IDLE_WAKEUPS | REFRESH_TYPE_HANDLES |
    REFRESH_TYPE_START_TIME | REFRESH_TYPE_CPU_TIME | REFRESH_TYPE_PRIORITY |
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    REFRESH_TYPE_FD_COUNT |
#endif
    REFRESH_TYPE_HARD_FAULTS;

class SharedSampler;

// Defines a group of tasks tracked by the task manager which belong to the same
// process. This class lives on the UI thread.
class TaskGroup {
 public:
  TaskGroup(
      base::ProcessHandle proc_handle,
      base::ProcessId proc_id,
      bool is_running_in_vm,
      const base::RepeatingClosure& on_background_calculations_done,
      const scoped_refptr<SharedSampler>& shared_sampler,
#if BUILDFLAG(IS_CHROMEOS_ASH)
      CrosapiTaskProviderAsh* crosapi_task_provider,
#endif
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner);
  TaskGroup(const TaskGroup&) = delete;
  TaskGroup& operator=(const TaskGroup&) = delete;
  ~TaskGroup();

  // Adds and removes the given |task| to this group. |task| must be running on
  // the same process represented by this group.
  void AddTask(Task* task);
  void RemoveTask(Task* task);

  void Refresh(const gpu::VideoMemoryUsageStats& gpu_memory_stats,
               base::TimeDelta update_interval,
               int64_t refresh_flags);

  Task* GetTaskById(TaskId task_id) const;

  // This is to be called after the task manager had informed its observers with
  // OnTasksRefreshedWithBackgroundCalculations() to begin another cycle for
  // this notification type.
  void ClearCurrentBackgroundCalculationsFlags();

  // True if all enabled background operations calculating resource usage of the
  // process represented by this TaskGroup have completed.
  bool AreBackgroundCalculationsDone() const;

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void SetArcSampler(ArcSharedSampler* sampler);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  const base::ProcessHandle& process_handle() const { return process_handle_; }
  const base::ProcessId& process_id() const { return process_id_; }

  const std::vector<raw_ptr<Task, VectorExperimental>>& tasks() const {
    return tasks_;
  }
  size_t num_tasks() const { return tasks().size(); }
  bool empty() const { return tasks().empty(); }

  double platform_independent_cpu_usage() const {
    return platform_independent_cpu_usage_;
  }
  void set_platform_independent_cpu_usage(double cpu_usage) {
    platform_independent_cpu_usage_ = cpu_usage;
  }
  base::Time start_time() const { return start_time_; }
  base::TimeDelta cpu_time() const { return cpu_time_; }
  void set_footprint_bytes(int64_t footprint) { memory_footprint_ = footprint; }
  int64_t footprint_bytes() const { return memory_footprint_; }
#if BUILDFLAG(IS_CHROMEOS)
  // Calculates swapped memory for Lacros too, so that we don't have to
  // re-calculate it in ash.
  int64_t swapped_bytes() const { return swapped_mem_bytes_; }
  void set_swapped_bytes(int64_t swapped_bytes) {
    swapped_mem_bytes_ = swapped_bytes;
  }
#endif  // BUILDFLAG(IS_CHROMEOS)
  int64_t gpu_memory() const { return gpu_memory_; }
  void set_gpu_memory(int64_t gpu_mem_bytes) { gpu_memory_ = gpu_mem_bytes; }
  bool gpu_memory_has_duplicates() const { return gpu_memory_has_duplicates_; }
  void set_gpu_memory_has_duplicates(bool has_duplicates) {
    gpu_memory_has_duplicates_ = has_duplicates;
  }
  int64_t per_process_network_usage_rate() const {
    return per_process_network_usage_rate_;
  }
  int64_t cumulative_per_process_network_usage() const {
    return cumulative_per_process_network_usage_;
  }
  bool is_backgrounded() const { return is_backgrounded_; }
  void set_is_backgrounded(bool is_backgrounded) {
    is_backgrounded_ = is_backgrounded;
  }

#if BUILDFLAG(IS_WIN)
  int64_t gdi_current_handles() const { return gdi_current_handles_; }
  int64_t gdi_peak_handles() const { return gdi_peak_handles_; }
  int64_t user_current_handles() const { return user_current_handles_; }
  int64_t user_peak_handles() const { return user_peak_handles_; }
  int64_t hard_faults_per_second() const { return hard_faults_per_second_; }
#endif  // BUILDFLAG(IS_WIN)

#if BUILDFLAG(ENABLE_NACL)
  int nacl_debug_stub_port() const { return nacl_debug_stub_port_; }
  void set_nacl_debug_stub_port(int stub_port) {
    nacl_debug_stub_port_ = stub_port;
  }
#endif  // BUILDFLAG(ENABLE_NACL)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  int open_fd_count() const { return open_fd_count_; }
  void set_open_fd_count(int open_fd_count) { open_fd_count_ = open_fd_count; }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

  int idle_wakeups_per_second() const { return idle_wakeups_per_second_; }
  void set_idle_wakeups_per_second(int idle_wakeups) {
    idle_wakeups_per_second_ = idle_wakeups;
  }

 private:
  void RefreshGpuMemory(const gpu::VideoMemoryUsageStats& gpu_memory_stats);

  void RefreshWindowsHandles();

#if BUILDFLAG(ENABLE_NACL)
  // |child_process_unique_id| see Task::GetChildProcessUniqueID().
  void RefreshNaClDebugStubPort(int child_process_unique_id);
  void OnRefreshNaClDebugStubPortDone(int port);
#endif
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  void OnOpenFdCountRefreshDone(int open_fd_count);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

  void OnCpuRefreshDone(double cpu_usage);
  void OnSwappedMemRefreshDone(int64_t swapped_mem_bytes);
  void OnProcessPriorityDone(base::Process::Priority priority);
  void OnIdleWakeupsRefreshDone(int idle_wakeups_per_second);

  void OnSamplerRefreshDone(
      std::optional<SharedSampler::SamplingResult> results);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  void OnArcSamplerRefreshDone(
      std::optional<ArcSharedSampler::MemoryFootprintBytes> results);
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)

  void OnBackgroundRefreshTypeFinished(int64_t finished_refresh_type);

  // The process' handle and ID.
  base::ProcessHandle process_handle_;
  base::ProcessId process_id_;
  bool is_running_in_vm_;

  // This is a callback into the TaskManagerImpl to inform it that the
  // background calculations for this TaskGroup has finished.
  const base::RepeatingClosure on_background_calculations_done_;

  scoped_refptr<TaskGroupSampler> worker_thread_sampler_;

  scoped_refptr<SharedSampler> shared_sampler_;
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // Shared sampler that retrieves memory footprint for all ARC processes.
  raw_ptr<ArcSharedSampler> arc_shared_sampler_;           // Not owned
  raw_ptr<CrosapiTaskProviderAsh> crosapi_task_provider_;  // Not owned
#endif                                             // BUILDFLAG(IS_CHROMEOS_ASH)

  // Lists the Tasks in this TaskGroup.
  // Tasks are not owned by the TaskGroup. They're owned by the TaskProviders.
  std::vector<raw_ptr<Task, VectorExperimental>> tasks_;

  // Flags will be used to determine when the background calculations has
  // completed for the enabled refresh types for this TaskGroup.
  int64_t expected_on_bg_done_flags_;
  int64_t current_on_bg_done_flags_;

  // The per process resources usages.
  double platform_independent_cpu_usage_;
  base::Time start_time_;     // Only calculated On Windows now.
  base::TimeDelta cpu_time_;  // Only calculated On Windows now.
  int64_t swapped_mem_bytes_;
  int64_t memory_footprint_;
  int64_t gpu_memory_;
  // The network usage in bytes per second as the sum of all network usages of
  // the individual tasks sharing the same process.
  int64_t per_process_network_usage_rate_;

  // A continuously updating sum of all bytes that have been downloaded and
  // uploaded by all tasks in this process.
  int64_t cumulative_per_process_network_usage_;

#if BUILDFLAG(IS_WIN)
  // Windows GDI and USER Handles.
  int64_t gdi_current_handles_;
  int64_t gdi_peak_handles_;
  int64_t user_current_handles_;
  int64_t user_peak_handles_;
  int64_t hard_faults_per_second_;
#endif  // BUILDFLAG(IS_WIN)
#if BUILDFLAG(ENABLE_NACL)
  int nacl_debug_stub_port_;
#endif  // BUILDFLAG(ENABLE_NACL)
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  // The number of file descriptors currently open by the process.
  int open_fd_count_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  int idle_wakeups_per_second_;
  bool gpu_memory_has_duplicates_;
  bool is_backgrounded_;

  // Always keep this the last member of this class so that it's the first to be
  // destroyed.
  base::WeakPtrFactory<TaskGroup> weak_ptr_factory_{this};
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_H_
