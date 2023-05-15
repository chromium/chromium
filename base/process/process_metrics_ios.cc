// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <limits.h>
#include <mach/task.h>
#include <mach/vm_region.h>
#include <malloc/malloc.h>
#include <stddef.h>

#include "base/check_op.h"
#include "base/memory/ptr_util.h"
#include "base/numerics/safe_conversions.h"
#include "build/blink_buildflags.h"
#include "process_metrics_apple_internal.h"

namespace base {

ProcessMetrics::ProcessMetrics(ProcessHandle process) {
  process_metrics_helper_ =
      std::make_unique<ProcessMetricsAppleInternal>(process);
}

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process) {
  return WrapUnique(new ProcessMetrics(process));
}

TimeDelta ProcessMetrics::GetCumulativeCPUUsage() {
  return process_metrics_helper_->GetCumulativeCPUUsage();
}

// The blink code path pulls in process_metrics_posix.cc which
// is used for the following implementations.
#if !BUILDFLAG(USE_BLINK)

size_t GetMaxFds() {
  static const rlim_t kSystemDefaultMaxFds = 256;
  rlim_t max_fds;
  struct rlimit nofile;
  if (getrlimit(RLIMIT_NOFILE, &nofile)) {
    // Error case: Take a best guess.
    max_fds = kSystemDefaultMaxFds;
  } else {
    max_fds = nofile.rlim_cur;
  }

  if (max_fds > INT_MAX)
    max_fds = INT_MAX;

  return static_cast<size_t>(max_fds);
}

void IncreaseFdLimitTo(unsigned int max_descriptors) {
  // Unimplemented.
}

#endif  // !BUILDFLAG(USE_BLINK)

}  // namespace base
