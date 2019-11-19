// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_

#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/power/cpu_data_collector.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace power_manager {
class PowerSupplyProperties;
}

namespace chromeos {

// A class which starts collecting power metrics, like the battery charge, as
// soon as it is initialized via Initialize().
//
// This class is implemented as a global singleton, initialized after
// DBusThreadManager which it depends on.
class PowerDataCollector : public PowerManagerClient::Observer {
 public:
  struct PowerSupplySample {
    PowerSupplySample();

    // Time when the sample was captured. We use base::Time instead of
    // base::TimeTicks because the latter does not advance when the system is
    // suspended.
    base::Time time;

    // True if connected to external power at the time of the sample.
    bool external_power;

    // The battery charge as a percentage of full charge in range [0.0, 100.00].
    double battery_percent;

    // The battery discharge rate in W. Positive if the battery is being
    // discharged and negative if it's being charged.
    double battery_discharge_rate;
  };

  struct SystemResumedSample {
    SystemResumedSample();

    // Time when the system resumed.
    base::Time time;

    // The duration for which the system was in sleep/suspend state.
    base::TimeDelta sleep_duration;
  };

  const base::circular_deque<PowerSupplySample>& power_supply_data() const {
    return power_supply_data_;
  }

  const base::circular_deque<SystemResumedSample>& system_resumed_data() const {
    return system_resumed_data_;
  }

  const CpuDataCollector& cpu_data_collector() const {
    return cpu_data_collector_;
  }

  // Can be called only after DBusThreadManager is initialized.
  static void Initialize();

  // Same as Initialize, but does not start the CpuDataCollector.
  static void InitializeForTesting();

  // Can be called only if initialized via Initialize, and before
  // DBusThreadManager is destroyed.
  static void Shutdown();

  // Returns the global instance of PowerDataCollector.
  static PowerDataCollector* Get();

  // PowerManagerClient::Observer implementation:
  void PowerChanged(const power_manager::PowerSupplyProperties& prop) override;
  void SuspendDone(const base::TimeDelta& sleep_duration) override;

  // Only those power data samples which fall within the last
  // |kSampleTimeLimitSec| are stored in memory.
  static const int kSampleTimeLimitSec;

 private:
  explicit PowerDataCollector(const bool start_cpu_data_collector);

  ~PowerDataCollector() override;

  base::circular_deque<PowerSupplySample> power_supply_data_;
  base::circular_deque<SystemResumedSample> system_resumed_data_;
  CpuDataCollector cpu_data_collector_;

  DISALLOW_COPY_AND_ASSIGN(PowerDataCollector);
};

// Adds |sample| to |sample_deque|.
// It dumps samples |PowerDataCollector::kSampleTimeLimitSec| or more older than
// |sample|.
template <typename SampleType>
void AddSample(base::circular_deque<SampleType>* sample_queue,
               const SampleType& sample) {
  while (!sample_queue->empty()) {
    const SampleType& first = sample_queue->front();
    if (sample.time - first.time >
        base::TimeDelta::FromSeconds(PowerDataCollector::kSampleTimeLimitSec)) {
      sample_queue->pop_front();
    } else {
      break;
    }
  }
  sample_queue->push_back(sample);
}

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_POWER_DATA_COLLECTOR_H_
