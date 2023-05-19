// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/power/coalition_resource_usage_provider_mac.h"

#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "components/power_metrics/mach_time_mac.h"
#include "components/power_metrics/resource_coalition_mac.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

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
  long_interval_begin_time_ = base::TimeTicks::Now();
  long_interval_begin_sample_ = std::move(sample);
}

void CoalitionResourceUsageProvider::StartShortInterval() {
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif
  DCHECK(short_interval_begin_time_.is_null());
  DCHECK(!short_interval_begin_sample_);

  if (!coalition_id_.has_value())
    return;

  short_interval_begin_time_ = base::TimeTicks::Now();
  short_interval_begin_sample_ =
      GetCoalitionResourceUsage(coalition_id_.value());
}

void CoalitionResourceUsageProvider::EndIntervals(
    absl::optional<power_metrics::CoalitionResourceUsageRate>*
        short_interval_resource_usage_rate,
    absl::optional<power_metrics::CoalitionResourceUsageRate>*
        long_interval_resource_usage_rate) {
#if DCHECK_IS_ON()
  DCHECK(initialized_);
#endif

  if (!coalition_id_.has_value())
    return;

  const base::TimeTicks now = base::TimeTicks::Now();
  std::unique_ptr<coalition_resource_usage> sample =
      GetCoalitionResourceUsage(coalition_id_.value());

  // `sample`, `short_interval_begin_sample_` or `long_interval_begin_sample_`
  // can be nullptr if the kernel couldn't allocate memory.

  if (sample) {
    DCHECK(!short_interval_begin_time_.is_null());
    DCHECK(!long_interval_begin_time_.is_null());

    if (short_interval_begin_sample_) {
      *short_interval_resource_usage_rate =
          power_metrics::GetCoalitionResourceUsageRate(
              *short_interval_begin_sample_, *sample,
              now - short_interval_begin_time_, timebase_,
              energy_impact_coefficients_);
    }

    if (long_interval_begin_sample_) {
      *long_interval_resource_usage_rate =
          power_metrics::GetCoalitionResourceUsageRate(
              *long_interval_begin_sample_, *sample,
              now - long_interval_begin_time_, timebase_,
              energy_impact_coefficients_);
    }
  }

  short_interval_begin_sample_.reset();
  short_interval_begin_time_ = base::TimeTicks();

  long_interval_begin_sample_.swap(sample);
  long_interval_begin_time_ = now;
}

std::unique_ptr<coalition_resource_usage>
CoalitionResourceUsageProvider::GetCoalitionResourceUsage(
    int64_t coalition_id) {
  return power_metrics::GetCoalitionResourceUsage(coalition_id);
}
