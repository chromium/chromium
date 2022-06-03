// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/process_metrics_history.h"

#include <limits>

#include "base/check_op.h"
#include "base/process/process_metrics.h"

#if defined(OS_MAC)
#include "content/public/browser/browser_child_process_host.h"
#endif

namespace performance_monitor {

ProcessMetricsHistory::ProcessMetricsHistory() = default;

ProcessMetricsHistory::~ProcessMetricsHistory() = default;

void ProcessMetricsHistory::Initialize(const ProcessMetadata& process_data,
                                       int initial_update_sequence) {
  DCHECK_EQ(base::kNullProcessHandle, process_data_.handle);
  process_data_ = process_data;
  last_update_sequence_ = initial_update_sequence;

#if defined(OS_MAC)
  process_metrics_ = base::ProcessMetrics::CreateProcessMetrics(
      process_data_.handle,
      content::BrowserChildProcessHost::GetPortProvider());
#else
  process_metrics_ =
      base::ProcessMetrics::CreateProcessMetrics(process_data_.handle);
#endif
}

ProcessMonitor::Metrics ProcessMetricsHistory::SampleMetrics() {
  ProcessMonitor::Metrics metrics;

  metrics.cpu_usage = process_metrics_->GetPlatformIndependentCPUUsage();
#if defined(OS_MAC) || defined(OS_LINUX) || defined(OS_CHROMEOS) || \
    defined(OS_AIX)
  metrics.idle_wakeups = process_metrics_->GetIdleWakeupsPerSecond();
#endif
#if defined(OS_MAC)
  metrics.package_idle_wakeups =
      process_metrics_->GetPackageIdleWakeupsPerSecond();
  metrics.energy_impact = process_metrics_->GetEnergyImpact();
#endif

  return metrics;
}

}  // namespace performance_monitor
