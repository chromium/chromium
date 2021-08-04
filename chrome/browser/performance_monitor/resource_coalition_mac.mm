// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <libproc.h>
#include <cstdint>

#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/time/time.h"
#include "chrome/browser/performance_monitor/resource_coalition_internal_types_mac.h"

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
  DCHECK_EQ(new_sample.cpu_time_eqos_len,
            static_cast<uint64_t>(COALITION_NUM_THREAD_QOS_TYPES));
  // Check for an overflow in the QoS data.
  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    if (new_sample.cpu_time_eqos[i] < old_sample.cpu_time_eqos[i]) {
      new_samples_exceeds_or_equals_old_ones = false;
      break;
    }
  }
  if (!new_samples_exceeds_or_equals_old_ones)
    return absl::nullopt;

  ResourceCoalition::DataRate ret;

  auto get_rate_per_second = [&interval_length](uint64_t new_sample,
                                                uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    uint64_t diff = new_sample - old_sample;
    return diff / interval_length.InSecondsF();
  };

  auto get_timedelta_rate_per_second =
      [&interval_length](uint64_t new_sample, uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    base::TimeDelta time_delta =
        base::TimeDelta::FromNanoseconds(new_sample - old_sample);
    return time_delta.InSecondsF() / interval_length.InSecondsF();
  };

  ret.cpu_time_per_second =
      get_timedelta_rate_per_second(new_sample.cpu_time, old_sample.cpu_time);
  ret.interrupt_wakeups_per_second = get_rate_per_second(
      new_sample.interrupt_wakeups, old_sample.interrupt_wakeups);
  ret.platform_idle_wakeups_per_second = get_rate_per_second(
      new_sample.platform_idle_wakeups, old_sample.platform_idle_wakeups);
  ret.bytesread_per_second =
      get_rate_per_second(new_sample.bytesread, old_sample.bytesread);
  ret.byteswritten_per_second =
      get_rate_per_second(new_sample.byteswritten, old_sample.byteswritten);
  ret.gpu_time_per_second =
      get_timedelta_rate_per_second(new_sample.gpu_time, old_sample.gpu_time);
  ret.power_nw = get_rate_per_second(new_sample.energy, old_sample.energy);

  for (int i = 0; i < COALITION_NUM_THREAD_QOS_TYPES; ++i) {
    ret.qos_time_per_second[i] = get_timedelta_rate_per_second(
        new_sample.cpu_time_eqos[i], old_sample.cpu_time_eqos[i]);
  }

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
  return GetDataRateImpl(GetResourceUsageData(coalition_id_.value()),
                         base::TimeTicks::Now());
}

absl::optional<ResourceCoalition::DataRate>
ResourceCoalition::GetDataRateFromFakeDataForTesting(
    std::unique_ptr<coalition_resource_usage> old_data_sample,
    std::unique_ptr<coalition_resource_usage> recent_data_sample,
    base::TimeDelta interval_length) {
  last_data_sample_.swap(old_data_sample);
  auto now = base::TimeTicks::Now();
  last_data_sample_timestamp_ = now - interval_length;
  return GetDataRateImpl(std::move(recent_data_sample), now);
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

absl::optional<ResourceCoalition::DataRate> ResourceCoalition::GetDataRateImpl(
    std::unique_ptr<coalition_resource_usage> new_data_sample,
    base::TimeTicks now) {
  auto ret =
      GetCoalitionDataDiff(*new_data_sample.get(), *last_data_sample_.get(),
                           now - last_data_sample_timestamp_);
  last_data_sample_.swap(new_data_sample);
  last_data_sample_timestamp_ = now;

  return ret;
}

}  // namespace performance_monitor
