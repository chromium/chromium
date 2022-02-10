// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/power_metrics/mach_time_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"

namespace {

// Details about whether it's possible to get coalition resource usage data on
// the system. This enum is used to report UMA. Do not reorder. Add additional
// values at the end and update CoalitionIDAvailability in enums.xml.
enum class CoalitionAvailability {
  kAvailable = 0,
  kCoalitionIDNotAvailable = 1,
  kCoalitionResourceUsageNotAvailable = 2,
  kUnabletoGetParentCoalitionId = 3,
  kNotAloneInCoalition = 4,
  kMaxValue = kNotAloneInCoalition
};

void ReportCoalitionAvailability(CoalitionAvailability availability) {
  base::UmaHistogramEnumeration(
      "PerformanceMonitor.ResourceCoalition.Availability", availability);
}

}  // namespace

CoalitionResourceUsageProvider::CoalitionResourceUsageProvider()
    : timebase_(power_metrics::GetSystemMachTimeBase()),
      energy_impact_coefficients_(
          power_metrics::ReadCoefficientsForCurrentMachineOrDefault()) {}

CoalitionResourceUsageProvider::~CoalitionResourceUsageProvider() = default;

void CoalitionResourceUsageProvider::Init() {
  DCHECK(!coalition_id_.has_value());
#if DCHECK_IS_ON()
  DCHECK(!initialized_);
  initialized_ = true;
#endif

  auto cid = power_metrics::GetProcessCoalitionId(base::GetCurrentProcId());

  if (!cid.has_value()) {
    ReportCoalitionAvailability(
        CoalitionAvailability::kCoalitionIDNotAvailable);
    return;
  }

  // Check if resource usage metrics can be retrieved for this coalition ID.
  std::unique_ptr<coalition_resource_usage> sample =
      GetCoalitionResourceUsage(cid.value());
  if (!sample) {
    ReportCoalitionAvailability(
        CoalitionAvailability::kCoalitionResourceUsageNotAvailable);
    return;
  }

  auto parent_cid = power_metrics::GetProcessCoalitionId(
      base::GetParentProcessId(base::GetCurrentProcessHandle()));

  if (!parent_cid.has_value()) {
    ReportCoalitionAvailability(
        CoalitionAvailability::kUnabletoGetParentCoalitionId);
    return;
  }

  // Do not report metrics if the coalition ID is shared with the parent
  // process.
  if (parent_cid.value() == cid.value() && !ignore_not_alone_for_testing_) {
    ReportCoalitionAvailability(CoalitionAvailability::kNotAloneInCoalition);
    return;
  }

  ReportCoalitionAvailability(CoalitionAvailability::kAvailable);
  coalition_id_ = cid.value();
  last_sample_ = std::move(sample);
  last_sample_time_ = base::TimeTicks::Now();
}

absl::optional<power_metrics::CoalitionResourceUsageRate>
CoalitionResourceUsageProvider::GetCoalitionResourceUsageRate() {
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  if (!coalition_id_.has_value())
    return absl::nullopt;

  DCHECK(last_sample_);
  DCHECK(!last_sample_time_.is_null());

  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta interval_duration = now - last_sample_time_;
  std::unique_ptr<coalition_resource_usage> sample =
      GetCoalitionResourceUsage(coalition_id_.value());

  auto rate = power_metrics::GetCoalitionResourceUsageRate(
      *last_sample_, *sample, interval_duration, timebase_,
      energy_impact_coefficients_);

  last_sample_.swap(sample);
  last_sample_time_ = now;

  return rate;
}

std::unique_ptr<coalition_resource_usage>
CoalitionResourceUsageProvider::GetCoalitionResourceUsage(
    int64_t coalition_id) {
  return power_metrics::GetCoalitionResourceUsage(coalition_id);
}
