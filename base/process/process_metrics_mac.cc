// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/process_metrics.h"

#include <AvailabilityMacros.h>
#include <libproc.h>
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <mach/mach_vm.h>
#include <mach/shared_region.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/sysctl.h>
#include <memory>

#include "base/logging.h"
#include "base/mac/mac_util.h"
#include "base/mac/mach_logging.h"
#include "base/memory/ptr_util.h"
#include "base/process/process_metrics_iocounters.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace {

// This is a standin for the private pm_task_energy_data_t struct.
struct OpaquePMTaskEnergyData {
  // Empirical size of the private struct.
  uint8_t data[408];
};

// Sample everything but network usage, since fetching network
// usage can hang.
constexpr uint8_t kPMSampleFlags = 0xff & ~0x8;

}  // namespace

extern "C" {

// From libpmsample.dylib
int pm_sample_task(mach_port_t task,
                   OpaquePMTaskEnergyData* pm_energy,
                   uint64_t mach_time,
                   uint8_t flags);

// From libpmenergy.dylib
double pm_energy_impact(OpaquePMTaskEnergyData* pm_energy);

}  // extern "C"

namespace base {

namespace {

double GetEnergyImpactInternal(mach_port_t task, uint64_t mach_time) {
  OpaquePMTaskEnergyData energy_info{};

  if (pm_sample_task(task, &energy_info, mach_time, kPMSampleFlags) != 0) {
    return 0.0;
  }
  return pm_energy_impact(&energy_info);
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

int ProcessMetrics::GetOpenFdCount() const {
  // In order to get a true count of the open number of FDs, PROC_PIDLISTFDS
  // is used. This is done twice: first to get the appropriate size of a
  // buffer, and then secondly to fill the buffer with the actual FD info.
  //
  // The buffer size returned in the first call is an estimate, based on the
  // number of allocated fileproc structures in the kernel. This number can be
  // greater than the actual number of open files, since the structures are
  // allocated in slabs. The value returned in proc_bsdinfo::pbi_nfiles is
  // also the number of allocated fileprocs, not the number in use.
  //
  // However, the buffer size returned in the second call is an accurate count
  // of the open number of descriptors. The contents of the buffer are unused.
  int rv = proc_pidinfo(process_, PROC_PIDLISTFDS, 0, nullptr, 0);
  if (rv < 0) {
    return -1;
  }

  std::unique_ptr<char[]> buffer(new char[static_cast<size_t>(rv)]);
  rv = proc_pidinfo(process_, PROC_PIDLISTFDS, 0, buffer.get(), rv);
  if (rv < 0) {
    return -1;
  }
  return static_cast<int>(static_cast<unsigned long>(rv) / PROC_PIDLISTFD_SIZE);
}

bool ProcessMetrics::GetIOCounters(IoCounters* io_counters) const {
  return false;
}

}  // namespace base
