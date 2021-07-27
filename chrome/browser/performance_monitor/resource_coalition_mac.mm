// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <libproc.h>
#include <cstdint>

#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"

// Comes from osfmk/mach/coalition.h

#define COALITION_TYPE_RESOURCE (0)
#define COALITION_TYPE_JETSAM (1)
#define COALITION_TYPE_MAX (1)

#define COALITION_NUM_TYPES (COALITION_TYPE_MAX + 1)

#define COALITION_NUM_THREAD_QOS_TYPES 7

// Comes from bsd/sys/coalition.h
//
// TODO(crbug.com/1229686): Report some data derived from the tasks_started and
// tasks_exited counters.
//
// TODO(crbug.com/1230152): Report some QoS numbers.
struct coalition_resource_usage {
  uint64_t tasks_started;
  uint64_t tasks_exited;
  uint64_t time_nonempty;
  uint64_t cpu_time;
  uint64_t interrupt_wakeups;
  uint64_t platform_idle_wakeups;
  uint64_t bytesread;
  uint64_t byteswritten;
  uint64_t gpu_time;
  uint64_t cpu_time_billed_to_me;
  uint64_t cpu_time_billed_to_others;
  uint64_t energy;
  uint64_t logical_immediate_writes;
  uint64_t logical_deferred_writes;
  uint64_t logical_invalidated_writes;
  uint64_t logical_metadata_writes;
  uint64_t logical_immediate_writes_to_external;
  uint64_t logical_deferred_writes_to_external;
  uint64_t logical_invalidated_writes_to_external;
  uint64_t logical_metadata_writes_to_external;
  uint64_t energy_billed_to_me;
  uint64_t energy_billed_to_others;
  uint64_t cpu_ptime;
  uint64_t cpu_time_eqos_len; /* Stores the number of thread QoS types */
  uint64_t cpu_time_eqos[COALITION_NUM_THREAD_QOS_TYPES];
  uint64_t cpu_instructions;
  uint64_t cpu_cycles;
  uint64_t fs_metadata_writes;
  uint64_t pm_writes;
};

struct proc_pidcoalitioninfo {
  uint64_t coalition_id[COALITION_NUM_TYPES];
  uint64_t reserved1;
  uint64_t reserved2;
  uint64_t reserved3;
};

// Comes from bsd/sys/proc_info.h
#define PROC_PIDCOALITIONINFO 20

extern "C" int coalition_info_resource_usage(
    uint64_t cid,
    struct coalition_resource_usage* cru,
    size_t sz);

namespace performance_monitor {
namespace {

const char kCoalitionAvailabilityHistogram[] =
    "PerformanceMonitor.ResourceCoalition.Availability";

// Details about whether or not it's possible to get coalition resource usage
// data on the system.
// This enum is reporting in metrics. Do not reorder; add additional values at
// the end and update the CoalitionIDAvailability enum in enums.xml.
enum class CoalitionAvailability {
  kAvailable = 0,
  kCoalitionIDNotAvailable = 1,
  kCoalitionResourceUsageNotAvailable = 2,
  kUnabletoGetParentCoalitionId = 3,
  kNotAloneInCoalition = 4,
  kMaxValue = kNotAloneInCoalition
};

// Returns the coalition ID that a given process belongs to, or nullopt if this
// information isn't available.
absl::optional<uint64_t> GetProcessCoalitionId(const base::ProcessId pid) {
  proc_pidcoalitioninfo coalition_info = {};
  int res = proc_pidinfo(pid, PROC_PIDCOALITIONINFO, 0, &coalition_info,
                         sizeof(coalition_info));

  if (res != sizeof(coalition_info))
    return absl::nullopt;

  return coalition_info.coalition_id[COALITION_TYPE_RESOURCE];
}

// Returns the coalition ID that the current process belongs to. If this isn't
// available or deemed not usable (e.g. if the process is not alone in its
// coalition) this will return nullopt and |availability_details| will receive
// some details about why this failed, otherwise this will return the ID and
// |availability_details| will have a success value.
absl::optional<uint64_t> GetCurrentCoalitionId(
    CoalitionAvailability* availability_details) {
  DCHECK(availability_details);
  auto cid = GetProcessCoalitionId(base::GetCurrentProcId());

  if (!cid.has_value()) {
    *availability_details = CoalitionAvailability::kCoalitionIDNotAvailable;
    return absl::nullopt;
  }

  // Check if resource usage metrics can be retrieved for this coalition ID.
  coalition_resource_usage cru = {};
  uint64_t res = coalition_info_resource_usage(cid.value(), &cru, sizeof(cru));
  if (res != 0) {
    *availability_details =
        CoalitionAvailability::kCoalitionResourceUsageNotAvailable;
    return absl::nullopt;
  }

  auto parent_cid = GetProcessCoalitionId(
      base::GetParentProcessId(base::GetCurrentProcessHandle()));

  if (!parent_cid.has_value()) {
    *availability_details =
        CoalitionAvailability::kUnabletoGetParentCoalitionId;
    return absl::nullopt;
  }

  // Do not report metrics if the coalition ID is shared with the parent
  // process.
  if (parent_cid.value() == cid.value()) {
    *availability_details = CoalitionAvailability::kNotAloneInCoalition;
    return absl::nullopt;
  }

  *availability_details = CoalitionAvailability::kAvailable;
  return cid;
}

// Returns the resource usage coalition data for the given coalition ID.
// This assumes that resource coalition data are always available for a given
// coalition ID (i.e. the coalition has a lifetime that exceeds the usage of the
// ID).
std::unique_ptr<coalition_resource_usage> GetResourceUsageData(
    int64_t coalition_id) {
  auto cru = std::make_unique<coalition_resource_usage>();
  uint64_t res = coalition_info_resource_usage(
      coalition_id, cru.get(), sizeof(coalition_resource_usage));
  DCHECK_EQ(0U, res);

  return cru;
}

// Computes the diff between two coalition_resource_usage objects and stores the
// per-second change rate for each field in a ResourceCoalition::Data object
// that will then be returned. Returns nullopt if any of the samples has
// overflowed.
absl::optional<ResourceCoalition::DataRate> GetCoalitionDataDiff(
    const coalition_resource_usage& new_sample,
    const coalition_resource_usage& old_sample,
    base::TimeDelta interval_length) {
  bool new_samples_exceeds_or_equals_old_ones =
      std::tie(new_sample.cpu_time, new_sample.interrupt_wakeups,
               new_sample.platform_idle_wakeups, new_sample.bytesread,
               new_sample.byteswritten, new_sample.gpu_time,
               new_sample.energy) >=
      std::tie(old_sample.cpu_time, old_sample.interrupt_wakeups,
               old_sample.platform_idle_wakeups, old_sample.bytesread,
               old_sample.byteswritten, old_sample.gpu_time, old_sample.energy);
  if (!new_samples_exceeds_or_equals_old_ones)
    return absl::nullopt;

  ResourceCoalition::DataRate ret;

  auto get_rate_per_second = [&interval_length](uint64_t new_sample,
                                                uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    uint64_t diff = new_sample - old_sample;
    return diff / interval_length.InSecondsF();
  };

  ret.cpu_time_per_second =
      get_rate_per_second(new_sample.cpu_time, old_sample.cpu_time);
  ret.interrupt_wakeups_per_second = get_rate_per_second(
      new_sample.interrupt_wakeups, old_sample.interrupt_wakeups);
  ret.platform_idle_wakeups_per_second = get_rate_per_second(
      new_sample.platform_idle_wakeups, old_sample.platform_idle_wakeups);
  ret.bytesread_per_second =
      get_rate_per_second(new_sample.bytesread, old_sample.bytesread);
  ret.byteswritten_per_second =
      get_rate_per_second(new_sample.byteswritten, old_sample.byteswritten);
  ret.gpu_time_per_second =
      get_rate_per_second(new_sample.gpu_time, old_sample.gpu_time);
  ret.energy_nj_per_second =
      get_rate_per_second(new_sample.energy, old_sample.energy);
  return ret;
}

}  // namespace

ResourceCoalition::DataRate::DataRate() = default;
ResourceCoalition::DataRate::DataRate(const DataRate& other) = default;
ResourceCoalition::DataRate& ResourceCoalition::DataRate::operator=(
    const DataRate& other) = default;
ResourceCoalition::DataRate::~DataRate() = default;

ResourceCoalition::ResourceCoalition() {
  CoalitionAvailability availability_details;
  SetCoalitionId(GetCurrentCoalitionId(&availability_details));
  base::UmaHistogramEnumeration(kCoalitionAvailabilityHistogram,
                                availability_details);
}
ResourceCoalition::~ResourceCoalition() = default;

absl::optional<ResourceCoalition::DataRate> ResourceCoalition::GetDataRate() {
  DCHECK(IsAvailable());
  DCHECK_EQ(GetProcessCoalitionId(base::GetCurrentProcId()).value(),
            coalition_id_.value());
  DCHECK(last_data_sample_);
  auto new_data = GetResourceUsageData(coalition_id_.value());
  auto now = base::TimeTicks::Now();
  auto ret = GetCoalitionDataDiff(*new_data.get(), *last_data_sample_.get(),
                                  now - last_data_sample_timestamp_);
  last_data_sample_.swap(new_data);
  last_data_sample_timestamp_ = now;

  return ret;
}

void ResourceCoalition::SetCoalitionIDToCurrentProcessIdForTesting() {
  SetCoalitionId(GetProcessCoalitionId(base::GetCurrentProcId()));
}

void ResourceCoalition::SetCoalitionId(absl::optional<uint64_t> coalition_id) {
  coalition_id_ = coalition_id;
  if (coalition_id_.has_value()) {
    last_data_sample_ = GetResourceUsageData(coalition_id_.value());
    last_data_sample_timestamp_ = base::TimeTicks::Now();
  }
}

}  // namespace performance_monitor
