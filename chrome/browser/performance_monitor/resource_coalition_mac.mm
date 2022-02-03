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

absl::optional<power_metrics::CoalitionResourceUsageRate>
ResourceCoalition::GetDataRate() {
  DCHECK(IsAvailable());
  DCHECK_EQ(
      power_metrics::GetProcessCoalitionId(base::GetCurrentProcId()).value(),
      coalition_id_.value());
  DCHECK(last_data_sample_);
  return GetDataRateImpl(
      power_metrics::GetCoalitionResourceUsage(coalition_id_.value()),
      base::TimeTicks::Now());
}

absl::optional<power_metrics::CoalitionResourceUsageRate>
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

absl::optional<power_metrics::CoalitionResourceUsageRate>
ResourceCoalition::GetDataRateImpl(
    std::unique_ptr<coalition_resource_usage> new_data_sample,
    base::TimeTicks now) {
  auto ret = power_metrics::GetCoalitionResourceUsageRate(
      *last_data_sample_, *new_data_sample, now - last_data_sample_timestamp_,
      mach_timebase_, energy_impact_coefficients_);

  last_data_sample_.swap(new_data_sample);
  last_data_sample_timestamp_ = now;

  return ret;
}

}  // namespace performance_monitor
