// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MONITOR_RESOURCE_COALITION_MAC_H_
#define CHROME_BROWSER_PERFORMANCE_MONITOR_RESOURCE_COALITION_MAC_H_

#include <cstdint>

#include "base/files/file_path.h"
#include "base/time/time.h"
#include "components/power_metrics/energy_impact_mac.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

// Forward declaration of the structure used internally to track resource usage.
struct coalition_resource_usage;

namespace performance_monitor {

// Resource Coalition is an, undocumented, mechanism available in macOS that
// allows retrieving various performance data for a group of tasks. A resource
// coalition accrues resource usage metrics for the tasks within the coalition,
// including processes that have died.
//
// In practice, for Chrome, a coalition encapsulates the browser and all the
// child processes that exist or have existed.
//
// NOTE: Chrome could itself belong to a non-empty coalition if it's started
// from a terminal, in which case the data will be hard to interpret. This class
// tracks this and reports that the coalition data isn't available when it's
// the case.
//
// NOTE 2: As this is an undocumented API there's a lot of unknowns here and the
// data retrieved by this class is experimental only.
class ResourceCoalition {
 public:
  // The different QoSLevels, the value of each level has to match the thread
  // QoS values defined in osfmk/mach/thread_policy.h
  enum class QoSLevels : int {
    kDefault = 0,
    kMaintenance = 1,
    kBackground = 2,
    kUtility = 3,
    kLegacy = 4,
    kUserInitiated = 5,
    kUserInteractive = 6,
    kMaxValue = kUserInteractive,
  };

  // The data tracked by the coalition.
  // TODO(sebmarchand): This is only a subset of the available data, we should
  // probably record more data.
  struct DataRate {
    DataRate();
    DataRate(const DataRate& other);
    DataRate& operator=(const DataRate& other);
    ~DataRate();

    double cpu_time_per_second;
    double interrupt_wakeups_per_second;
    double platform_idle_wakeups_per_second;
    double bytesread_per_second;
    double byteswritten_per_second;
    double gpu_time_per_second;
    // Only makes sense on Intel macs, not computed on M1 macs.
    double energy_impact_per_second;
    // Only available on M1 macs as of September 2021.
    double power_nw;

    double qos_time_per_second[static_cast<int>(QoSLevels::kMaxValue) + 1];
  };

  // Note: The constructor will record whether or not coalition data are
  // available to UMA.
  ResourceCoalition();
  ResourceCoalition(const ResourceCoalition& other) = delete;
  ResourceCoalition& operator=(const ResourceCoalition& other) = delete;
  ~ResourceCoalition();

  // Returns true if coalition data is available.
  bool IsAvailable() { return coalition_id_.has_value(); }

  // Computes the change rate since the last time this function has been called
  // or since the creation of this object, returns nullopt if not available
  // or if one of the data counter has overflowed.
  // This should only be called if |IsAvailable| returns true.
  absl::optional<DataRate> GetDataRate();

 protected:
  void SetCoalitionIDToCurrentProcessIdForTesting();

  // Compute the data change rate between |old_data_sample| and
  // |recent_data_sample| over an interval of length |interval_length|.
  absl::optional<DataRate> GetDataRateFromFakeDataForTesting(
      std::unique_ptr<coalition_resource_usage> old_data_sample,
      std::unique_ptr<coalition_resource_usage> recent_data_sample,
      base::TimeDelta interval_length);

  // Initialize or reset the EI coefficients for testing.
  void SetEnergyImpactCoefficientsForTesting(
      const absl::optional<power_metrics::EnergyImpactCoefficients>&
          coefficients);

  // Override the machine time base for testing.
  void SetMachTimebaseForTesting(
      const mach_timebase_info_data_t& mach_timebase);

 private:
  void SetCoalitionId(absl::optional<uint64_t> coalition_id);

  // Computes the diff between two coalition_resource_usage objects and stores
  // the per-second change rate for each field in a ResourceCoalition::Data
  // object that will then be returned. Returns nullopt if any of the samples
  // has overflowed.
  absl::optional<DataRate> GetCoalitionDataDiff(
      const coalition_resource_usage& new_sample,
      const coalition_resource_usage& old_sample,
      base::TimeDelta interval_length);

  // Implementation details for GetDataRate.
  absl::optional<DataRate> GetDataRateImpl(
      std::unique_ptr<coalition_resource_usage> new_data_sample,
      base::TimeTicks now);

  // The coalition ID for the current process or nullopt if this isn't
  // available.
  absl::optional<uint64_t> coalition_id_;

  // Used to convert coalition_resource_usage time fields from
  // mach_absolute_time units to ns.
  mach_timebase_info_data_t mach_timebase_;

  // Coefficients to compute Energy Impact from a `coalition_resource_usage`.
  absl::optional<power_metrics::EnergyImpactCoefficients>
      energy_impact_coefficients_;

  // The data sample collected during the last call to GetDataDiff or since
  // creating this object.
  std::unique_ptr<coalition_resource_usage> last_data_sample_;

  // The timestamp associated with |last_data_sample_|.
  base::TimeTicks last_data_sample_timestamp_;
};

}  // namespace performance_monitor

#endif  // CHROME_BROWSER_PERFORMANCE_MONITOR_RESOURCE_COALITION_MAC_H_
