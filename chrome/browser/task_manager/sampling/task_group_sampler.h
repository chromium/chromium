// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_SAMPLER_H_
#define CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_SAMPLER_H_

#include <stdint.h>

#include <memory>

#include "base/functional/callback.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/task/sequenced_task_runner.h"
#include "build/build_config.h"

namespace task_manager {

// Defines the expensive process' stats sampler that will calculate these
// resources on the worker thread. Objects of this class are created by the
// TaskGroups on the UI thread, however it will be used mainly on a blocking
// pool thread.
class TaskGroupSampler : public base::RefCountedThreadSafe<TaskGroupSampler> {
 public:
  // Below are the types of callbacks that are invoked on the UI thread upon
  // completion of corresponding refresh tasks on the worker thread.
  using OnCpuRefreshCallback = base::RepeatingCallback<void(double)>;
  using OnSwappedMemRefreshCallback = base::RepeatingCallback<void(int64_t)>;
  using OnIdleWakeupsCallback = base::RepeatingCallback<void(int)>;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  using OnOpenFdCountCallback = base::RepeatingCallback<void(int)>;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  using OnProcessPriorityCallback =
      base::RepeatingCallback<void(base::Process::Priority)>;

  TaskGroupSampler(
      base::Process process,
      const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner,
      const OnCpuRefreshCallback& on_cpu_refresh,
      const OnSwappedMemRefreshCallback& on_memory_refresh,
      const OnIdleWakeupsCallback& on_idle_wakeups,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
      const OnOpenFdCountCallback& on_open_fd_count,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
      const OnProcessPriorityCallback& on_process_priority);

  TaskGroupSampler(const TaskGroupSampler&) = delete;
  TaskGroupSampler& operator=(const TaskGroupSampler&) = delete;

  // Refreshes the expensive process' stats (CPU usage, memory usage, and idle
  // wakeups per second) on the worker thread.
  void Refresh(int64_t refresh_flags);

 private:
  friend class base::RefCountedThreadSafe<TaskGroupSampler>;
  ~TaskGroupSampler();

  // The refresh calls that will be done on the worker thread.
  double RefreshCpuUsage();
  int64_t RefreshSwappedMem();
  int RefreshIdleWakeupsPerSecond();
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  int RefreshOpenFdCount();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  base::Process::Priority RefreshProcessPriority();

  // The process that holds the handle that we own so that we can use it for
  // creating the ProcessMetrics.
  base::Process process_;

  std::unique_ptr<base::ProcessMetrics> process_metrics_;

  // Keep track of whether or not we have real cpu usage. First call to
  // GetPlatformIndependentCPUUsage returns 0, which we treat as NaN.
  bool cpu_usage_calculated_ = false;

  // The specific blocking pool SequencedTaskRunner that will be used to post
  // the refresh tasks onto serially.
  scoped_refptr<base::SequencedTaskRunner> blocking_pool_runner_;

  // The UI-thread callbacks in TaskGroup to be called when their corresponding
  // refreshes on the worker thread are done.
  const OnCpuRefreshCallback on_cpu_refresh_callback_;
  const OnSwappedMemRefreshCallback on_swapped_mem_refresh_callback_;
  const OnIdleWakeupsCallback on_idle_wakeups_callback_;
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  const OnOpenFdCountCallback on_open_fd_count_callback_;
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  const OnProcessPriorityCallback on_process_priority_callback_;

  // To assert we're running on the correct thread.
  SEQUENCE_CHECKER(worker_pool_sequenced_checker_);
};

}  // namespace task_manager

#endif  // CHROME_BROWSER_TASK_MANAGER_SAMPLING_TASK_GROUP_SAMPLER_H_
