// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_MAC_H_
#define CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_MAC_H_

#include <mach/mach_time.h>
#include <stdint.h>
#include <vector>

#include "base/dcheck_is_on.h"
#include "base/gtest_prod_util.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace power_metrics {
struct CoalitionResourceUsageRate;
}

struct coalition_resource_usage;

// Provides resource usage rate for the current process' coalition.
//
// See //components/power_metrics/resource_coalition_mac.h for more details
// about resource coalitions.
//
// NOTE: Chrome could belong to a non-empty coalition if it's started from a
// terminal, in which case the data will be hard to interpret. This class
// reports that the coalition data isn't available when it's the case.
class CoalitionResourceUsageProvider {
 public:
  CoalitionResourceUsageProvider();
  virtual ~CoalitionResourceUsageProvider();

  // Initializes the coalition resource usage id. Reports whether it is
  // available to UMA. Must be invoked once before
  // GetCoalitionResourceUsageRate().
  void Init();

  // Returns resource usage rate for the current process' coalition, since the
  // last call to Init() or GetCoalitionResourceUsageRate(). Returns nullopt if
  // not available.
  absl::optional<power_metrics::CoalitionResourceUsageRate>
  GetCoalitionResourceUsageRate();

 protected:
  // Used to convert `coalition_resource_usage` time fields from
  // `mach_absolute_time` units to ns.
  mach_timebase_info_data_t timebase_;

  // If true, uses the coalition id even if the process is in the same coalition
  // as its parent.
  bool ignore_not_alone_for_testing_ = false;

 private:
  // Virtual for testing.
  virtual std::unique_ptr<coalition_resource_usage> GetCoalitionResourceUsage(
      int64_t coalition_id);

#if DCHECK_IS_ON()
  bool initialized_ = false;
#endif

  // Coalition ID for the current process or nullopt if not available.
  absl::optional<uint64_t> coalition_id_;

  // Coefficients to compute Energy Impact from a `coalition_resource_usage`.
  const absl::optional<power_metrics::EnergyImpactCoefficients>
      energy_impact_coefficients_;

  // Last coalition resource usage sample.
  std::unique_ptr<coalition_resource_usage> last_sample_;

  // Time at which `last_sample_` was sampled.
  base::TimeTicks last_sample_time_;
};

#endif  // CHROME_BROWSER_METRICS_POWER_COALITION_RESOURCE_USAGE_PROVIDER_MAC_H_
