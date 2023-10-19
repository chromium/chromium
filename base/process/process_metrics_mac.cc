// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <AvailabilityMacros.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <memory>

#include "base/apple/mach_logging.h"
#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics_iocounters.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace base {

namespace {

double GetEnergyImpactInternal(mach_port_t task, uint64_t mach_time) {
  return 0;
}

}  // namespace

// Getting a mach task from a pid for another process requires permissions in
// general, so there doesn't really seem to be a way to do these (and spinning
// up ps to fetch each stats seems dangerous to put in a base api for anyone to
// call). Child processes ipc their port, so return something if available,
// otherwise return 0.

// static
std::unique_ptr<ProcessMetrics> ProcessMetrics::CreateProcessMetrics(
    ProcessHandle process,
    PortProvider* port_provider) {
  return WrapUnique(new ProcessMetrics(process, port_provider));
}

ProcessMetrics::ProcessMetrics(ProcessHandle process,
                               PortProvider* port_provider)
    : process_(process),
      last_absolute_idle_wakeups_(0),
      last_absolute_package_idle_wakeups_(0),
      last_energy_impact_(0),
      port_provider_(port_provider) {}

int ProcessMetrics::GetEnergyImpact() {
  uint64_t now = mach_absolute_time();
  if (last_energy_impact_ == 0) {
    last_energy_impact_ = GetEnergyImpactInternal(TaskForPid(process_), now);
    last_energy_impact_time_ = now;
    return 0;
  }

  double total_energy_impact =
      GetEnergyImpactInternal(TaskForPid(process_), now);
  uint64_t delta = now - last_energy_impact_time_;
  if (delta == 0) {
    return 0;
  }

  // Scale by 100 since the histogram is integral.
  double seconds_since_last_measurement =
      base::TimeTicks::FromMachAbsoluteTime(delta).since_origin().InSecondsF();
  int energy_impact = 100 * (total_energy_impact - last_energy_impact_) /
                      seconds_since_last_measurement;
  last_energy_impact_ = total_energy_impact;
  last_energy_impact_time_ = now;

  return energy_impact;
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return false;
}

}  // namespace base
