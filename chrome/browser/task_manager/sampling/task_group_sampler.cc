// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/task_manager/sampling/task_group_sampler.h"

#include <limits>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/process/process_metrics.h"
#include "base/task/sequenced_task_runner.h"
#include "base/types/expected.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/task_manager/task_manager_observer.h"
#include "content/public/browser/browser_child_process_host.h"
#include "content/public/browser/browser_thread.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <psapi.h>
#endif

namespace task_manager {

namespace {

std::unique_ptr<base::ProcessMetrics> CreateProcessMetrics(
    base::ProcessHandle handle) {
#if !BUILDFLAG(IS_MAC)
  return base::ProcessMetrics::CreateProcessMetrics(handle);
#else
  return base::ProcessMetrics::CreateProcessMetrics(
      handle, content::BrowserChildProcessHost::GetPortProvider());
#endif
}

}  // namespace

TaskGroupSampler::TaskGroupSampler(
    base::Process process,
    const scoped_refptr<base::SequencedTaskRunner>& blocking_pool_runner,
    const OnCpuRefreshCallback& on_cpu_refresh,
    const OnSwappedMemRefreshCallback& on_swapped_mem_refresh,
    const OnIdleWakeupsCallback& on_idle_wakeups,
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    const OnOpenFdCountCallback& on_open_fd_count,
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
    const OnProcessPriorityCallback& on_process_priority)
    : process_(std::move(process)),
      process_metrics_(CreateProcessMetrics(process_.Handle())),
      blocking_pool_runner_(blocking_pool_runner),
      on_cpu_refresh_callback_(on_cpu_refresh),
      on_swapped_mem_refresh_callback_(on_swapped_mem_refresh),
      on_idle_wakeups_callback_(on_idle_wakeups),
#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
      on_open_fd_count_callback_(on_open_fd_count),
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
      on_process_priority_callback_(on_process_priority) {
  DCHECK(blocking_pool_runner.get());

  // This object will be created on the UI thread, however the sequenced checker
  // will be used to assert we're running the expensive operations on one of the
  // blocking pool threads.
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DETACH_FROM_SEQUENCE(worker_pool_sequenced_checker_);
}

void TaskGroupSampler::Refresh(int64_t refresh_flags) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  if (TaskManagerObserver::IsResourceRefreshEnabled(REFRESH_TYPE_CPU,
                                                    refresh_flags)) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&TaskGroupSampler::RefreshCpuUsage, this),
        base::BindOnce(on_cpu_refresh_callback_));
  }

  if (TaskManagerObserver::IsResourceRefreshEnabled(REFRESH_TYPE_SWAPPED_MEM,
                                                    refresh_flags)) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&TaskGroupSampler::RefreshSwappedMem, this),
        base::BindOnce(on_swapped_mem_refresh_callback_));
  }

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  if (TaskManagerObserver::IsResourceRefreshEnabled(REFRESH_TYPE_IDLE_WAKEUPS,
                                                    refresh_flags)) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&TaskGroupSampler::RefreshIdleWakeupsPerSecond, this),
        base::BindOnce(on_idle_wakeups_callback_));
  }
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
  if (TaskManagerObserver::IsResourceRefreshEnabled(REFRESH_TYPE_FD_COUNT,
                                                    refresh_flags)) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE, base::BindOnce(&TaskGroupSampler::RefreshOpenFdCount, this),
        base::BindOnce(on_open_fd_count_callback_));
  }
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

  if (TaskManagerObserver::IsResourceRefreshEnabled(REFRESH_TYPE_PRIORITY,
                                                    refresh_flags)) {
    blocking_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(&TaskGroupSampler::RefreshProcessPriority, this),
        base::BindOnce(on_process_priority_callback_));
  }
}

TaskGroupSampler::~TaskGroupSampler() {
}

double TaskGroupSampler::RefreshCpuUsage() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);
  // TODO(https://crbug.com/331250452): Errors are converted to 0.0 for
  // backwards compatibility. The values returned from this are surfaced by the
  // `chrome.processes` extension API, so changing this will need developer
  // outreach.
  double cpu_usage =
      process_metrics_->GetPlatformIndependentCPUUsage().value_or(0.0);
  if (!cpu_usage_calculated_) {
    // First successful call to GetPlatformIndependentCPUUsage returns 0. Ignore
    // it, and return NaN.
    cpu_usage_calculated_ = true;
    return std::numeric_limits<double>::quiet_NaN();
  }
  return cpu_usage;
}

int64_t TaskGroupSampler::RefreshSwappedMem() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);

#if BUILDFLAG(IS_CHROMEOS_ASH)
  return process_metrics_->GetVmSwapBytes();
#else
  return 0;
#endif  // BUILDFLAG(IS_CHROMEOS_ASH)
}

int TaskGroupSampler::RefreshIdleWakeupsPerSecond() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);

  return process_metrics_->GetIdleWakeupsPerSecond();
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)
int TaskGroupSampler::RefreshOpenFdCount() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);

  return process_metrics_->GetOpenFdCount();
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_MAC)

base::Process::Priority TaskGroupSampler::RefreshProcessPriority() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(worker_pool_sequenced_checker_);
#if BUILDFLAG(IS_MAC)
  if (process_.is_current()) {
    base::SelfPortProvider self_provider;
    return process_.GetPriority(&self_provider);
  }
  return process_.GetPriority(
      content::BrowserChildProcessHost::GetPortProvider());
#else
  return process_.GetPriority();
#endif  // BUILDFLAG(IS_MAC)
}

}  // namespace task_manager
