// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <stddef.h>
#include <stdint.h>
#include <sys/param.h>
#include <sys/sysctl.h>

#include "base/memory/ptr_util.h"
#include "base/types/expected.h"

namespace base {

namespace {

base::expected<int, ProcessCPUUsageError> GetProcessCPU(pid_t pid) {
  struct kinfo_proc info;
  size_t length;
  int mib[] = { CTL_KERN, KERN_PROC, KERN_PROC_PID, pid,
                sizeof(struct kinfo_proc), 0 };

  if (sysctl(mib, std::size(mib), NULL, &length, NULL, 0) < 0) {
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }

  mib[5] = (length / sizeof(struct kinfo_proc));

  if (sysctl(mib, std::size(mib), &info, &length, NULL, 0) < 0) {
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }

  return base::ok(info.p_pctcpu);
}

}  // namespace

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

base::expected<double, ProcessCPUUsageError>
ProcessMetrics::GetPlatformIndependentCPUUsage() {
  TimeTicks time = TimeTicks::Now();

  if (last_cpu_time_.is_zero()) {
    // First call, just set the last values.
    last_cpu_time_ = time;
    return base::ok(0.0);
  }

  const base::expected<int, ProcessCPUUsageError> cpu = GetProcessCPU(process_);
  if (!cpu.has_value()) {
    return base::unexpected(cpu.error());
  }

  last_cpu_time_ = time;
  return base::ok(double{cpu.value()} / FSCALE * 100.0);
}

base::expected<TimeDelta, ProcessCPUUsageError>
ProcessMetrics::GetCumulativeCPUUsage() {
  NOTREACHED();
}

ProcessMetrics::ProcessMetrics(ProcessHandle process)
    : process_(process),
      last_cpu_(0) {}

size_t GetSystemCommitCharge() {
  int mib[] = { CTL_VM, VM_METER };
  int pagesize;
  struct vmtotal vmtotal;
  unsigned long mem_total, mem_free, mem_inactive;
  size_t len = sizeof(vmtotal);

  if (sysctl(mib, std::size(mib), &vmtotal, &len, NULL, 0) < 0)
    return 0;

  mem_total = vmtotal.t_vm;
  mem_free = vmtotal.t_free;
  mem_inactive = vmtotal.t_vm - vmtotal.t_avm;

  pagesize = getpagesize();

  return mem_total - (mem_free*pagesize) - (mem_inactive*pagesize);
}

}  // namespace base
