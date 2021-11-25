// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/performance_monitor/resource_coalition_mac.h"

#include <Foundation/Foundation.h>
#include <libproc.h>
#include <cstdint>

#include "base/mac/foundation_util.h"
#include "base/mac/scoped_ioobject.h"
#include "base/metrics/histogram_functions.h"
#include "base/process/process_handle.h"
#include "base/strings/sys_string_conversions.h"
#include "base/time/time.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "components/power_metrics/mach_time_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"

namespace performance_monitor {
namespace {

static_assert(
    THREAD_QOS_DEFAULT ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kDefault),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(
    THREAD_QOS_MAINTENANCE ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kMaintenance),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(
    THREAD_QOS_BACKGROUND ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kBackground),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(
    THREAD_QOS_UTILITY ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kUtility),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(
    THREAD_QOS_LEGACY ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kLegacy),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(
    THREAD_QOS_USER_INITIATED ==
        static_cast<int>(
            performance_monitor::ResourceCoalition::QoSLevels::kUserInitiated),
    "QoSLevels indexes should match the OS defined ones.");
static_assert(THREAD_QOS_USER_INTERACTIVE ==
                  static_cast<int>(performance_monitor::ResourceCoalition::
                                       QoSLevels::kUserInteractive),
              "QoSLevels indexes should match the OS defined ones.");

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

// Returns the coalition ID that the current process belongs to. If this isn't
// available or deemed not usable (e.g. if the process is not alone in its
// coalition) this will return nullopt and |availability_details| will receive
// some details about why this failed, otherwise this will return the ID and
// |availability_details| will have a success value.
absl::optional<uint64_t> GetCurrentCoalitionId(
    CoalitionAvailability* availability_details) {
  DCHECK(availability_details);
  auto cid = power_metrics::GetProcessCoalitionId(base::GetCurrentProcId());

  if (!cid.has_value()) {
    *availability_details = CoalitionAvailability::kCoalitionIDNotAvailable;
    return absl::nullopt;
  }

  // Check if resource usage metrics can be retrieved for this coalition ID.
  if (!power_metrics::GetCoalitionResourceUsage(cid.value())) {
    *availability_details =
        CoalitionAvailability::kCoalitionResourceUsageNotAvailable;
    return absl::nullopt;
  }

  auto parent_cid = power_metrics::GetProcessCoalitionId(
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

}  // namespace

ResourceCoalition::DataRate::DataRate() = default;
ResourceCoalition::DataRate::DataRate(const DataRate& other) = default;
ResourceCoalition::DataRate& ResourceCoalition::DataRate::operator=(
    const DataRate& other) = default;
ResourceCoalition::DataRate::~DataRate() = default;

ResourceCoalition::ResourceCoalition()
    : mach_timebase_(power_metrics::GetSystemMachTimeBase()),
      energy_impact_coefficients_(
          power_metrics::ReadCoefficientsForCurrentMachineOrDefault()) {
  CoalitionAvailability availability_details;
  SetCoalitionId(GetCurrentCoalitionId(&availability_details));
  base::UmaHistogramEnumeration(kCoalitionAvailabilityHistogram,
                                availability_details);
}

ResourceCoalition::~ResourceCoalition() = default;

absl::optional<ResourceCoalition::DataRate> ResourceCoalition::GetDataRate() {
  DCHECK(IsAvailable());
  DCHECK_EQ(
      power_metrics::GetProcessCoalitionId(base::GetCurrentProcId()).value(),
      coalition_id_.value());
  DCHECK(last_data_sample_);
  return GetDataRateImpl(
      power_metrics::GetCoalitionResourceUsage(coalition_id_.value()),
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
  SetCoalitionId(
      power_metrics::GetProcessCoalitionId(base::GetCurrentProcId()));
}

void ResourceCoalition::SetEnergyImpactCoefficientsForTesting(
    const absl::optional<power_metrics::EnergyImpactCoefficients>&
        coefficients) {
  energy_impact_coefficients_ = coefficients;
}

void ResourceCoalition::SetMachTimebaseForTesting(
    const mach_timebase_info_data_t& mach_timebase) {
  mach_timebase_ = mach_timebase;
}

void ResourceCoalition::SetCoalitionId(absl::optional<uint64_t> coalition_id) {
  coalition_id_ = coalition_id;
  if (coalition_id_.has_value()) {
    last_data_sample_ =
        power_metrics::GetCoalitionResourceUsage(coalition_id_.value());
    last_data_sample_timestamp_ = base::TimeTicks::Now();
  }
}

absl::optional<ResourceCoalition::DataRate>
ResourceCoalition::GetCoalitionDataDiff(
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

  auto get_timedelta_rate_per_second = [&interval_length, self = this](
                                           uint64_t new_sample,
                                           uint64_t old_sample) -> double {
    DCHECK_GE(new_sample, old_sample);
    // Compute the delta in s, being careful to avoid truncation due to integral
    // division.
    double delta_sample_s =
        power_metrics::MachTimeToNs(new_sample - old_sample,
                                    self->mach_timebase_) /
        static_cast<double>(base::Time::kNanosecondsPerSecond);
    return delta_sample_s / interval_length.InSecondsF();
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

  if (energy_impact_coefficients_.has_value()) {
    ret.energy_impact_per_second =
        (power_metrics::ComputeEnergyImpactForResourceUsage(
             new_sample, energy_impact_coefficients_.value(), mach_timebase_) -
         power_metrics::ComputeEnergyImpactForResourceUsage(
             old_sample, energy_impact_coefficients_.value(), mach_timebase_)) /
        interval_length.InSecondsF();
  } else {
    // TODO(siggi): Use something else here as sentinel?
    ret.energy_impact_per_second = 0;
  }

  return ret;
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
