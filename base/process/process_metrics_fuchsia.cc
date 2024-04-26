// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <lib/fdio/limits.h>
#include <lib/zx/process.h>

#include "base/fuchsia/fuchsia_logging.h"
#include "base/memory/ptr_util.h"

namespace base {

size_t GetMaxFds() {
  return FDIO_MAX_FD;
}

size_t GetHandleLimit() {
  // Duplicated from the internal Magenta kernel constant kMaxHandleCount
  // (zircon/kernel/object/handle.cc).
  return 256 * 1024u;
}

size_t GetSystemCommitCharge() {
  // TODO(crbug.com/42050627): Fuchsia does not support this.
  return 0;
}

ProcessMetrics::ProcessMetrics(ProcessHandle process) : process_(process) {}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return base::WrapUnique(new ProcessMetrics(process));
}

base::expected<TimeDelta, ProcessCPUUsageError>
ProcessMetrics::GetCumulativeCPUUsage() {
  zx_info_task_runtime_t stats;

  zx_status_t status = zx::unowned_process(process_)->get_info(
      ZX_INFO_TASK_RUNTIME, &stats, sizeof(stats), nullptr, nullptr);
  if (status != ZX_OK) {
    return base::unexpected(ProcessCPUUsageError::kSystemError);
  }

  return base::ok(TimeDelta::FromZxDuration(stats.cpu_time));
}

bool GetSystemMemoryInfo(SystemMemoryInfoKB* meminfo) {
  // TODO(crbug.com/42050627).
  return false;
}

}  // namespace base
