// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_PROCESS_METRICS_APPLE_INTERNAL_H_
#define BASE_PROCESS_PROCESS_METRICS_APPLE_INTERNAL_H_

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/process/process_handle.h"
#include "base/process/process_metrics.h"
#include "build/build_config.h"

#include <mach/mach.h>
#include "base/process/port_provider_mac.h"

namespace base {

enum class MachVMRegionResult;

class BASE_EXPORT ProcessMetricsAppleInternal {
 public:
#if BUILDFLAG(IS_MAC)
  ProcessMetricsAppleInternal(ProcessHandle process,
                              PortProvider* port_provider);
#else
  explicit ProcessMetricsAppleInternal(ProcessHandle process);
#endif

  ProcessMetricsAppleInternal(const ProcessMetricsAppleInternal&) = delete;
  ProcessMetricsAppleInternal& operator=(const ProcessMetricsAppleInternal&) =
      delete;

  ~ProcessMetricsAppleInternal() = default;

  TimeDelta GetCumulativeCPUUsage();

  task_power_info GetTaskPowerInfo();

  // Queries the port provider if it's set.
  mach_port_t TaskForPid(ProcessHandle process) const;

 private:
#if BUILDFLAG(IS_MAC)
  raw_ptr<PortProvider> port_provider_;
#endif

  ProcessHandle process_;
};

}  // namespace base

#endif  // BASE_PROCESS_PROCESS_METRICS_APPLE_INTERNAL_H_
