// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_CPU_DATA_COLLECTOR_H_
#define CHROME_BROWSER_CHROMEOS_POWER_CPU_DATA_COLLECTOR_H_

#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/containers/circular_deque.h"
#include "base/files/file_util.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"

namespace chromeos {

// A class to sample CPU idle state occupancy and freq state occupancy.
// Collects raw data from sysfs and does not convert it to percentage
// occupancy. As CPUs can be offline at times, or the system can be suspended at
// other times, it is best for the consumer of this data to calculate percentage
// occupancy information using suspend time data from
// PowerDataCollector::system_resumed_data.
class CpuDataCollector {
 public:
  struct StateOccupancySample {
    StateOccupancySample();
    StateOccupancySample(const StateOccupancySample& other);
    ~StateOccupancySample();

    // The time when the data was sampled.
    base::Time time;

    // Indicates whether the CPU is online.
    bool cpu_online;

    // A mapping from a CPU state to time spent in that state. For idle state
    // samples, the name of the state at index i in this vector is the name at
    // index i in the vector returned by cpu_idle_state_names(). Similarly, for
    // freq state occupancy, similar information is in the vector returned by
    // cpu_freq_state_names().
    std::vector<base::TimeDelta> time_in_state;
  };

  using StateOccupancySampleDeque = base::circular_deque<StateOccupancySample>;

  const std::vector<std::string>& cpu_idle_state_names() const {
    return cpu_idle_state_names_;
  }

  const std::vector<StateOccupancySampleDeque>& cpu_idle_state_data() const {
    return cpu_idle_state_data_;
  }

  const std::vector<std::string>& cpu_freq_state_names() const {
    return cpu_freq_state_names_;
  }

  const std::vector<StateOccupancySampleDeque>& cpu_freq_state_data() const {
    return cpu_freq_state_data_;
  }

  CpuDataCollector();
  ~CpuDataCollector();

  // Starts a repeating timer which periodically runs a callback to collect
  // CPU state occupancy samples.
  void Start();

  // Read CPU frequency data of a specific CPU from the file in |path| and fill
  // |cpu_freq_state_names| and |freq_sample| with the data.
  // Sample file looks like:
  // 126000 223344
  // 216000 93495
  // ...
  // 1800000 321907
  static bool ReadCpuFreqTimeInState(
      const base::FilePath& path,
      std::vector<std::string>* cpu_freq_state_names,
      CpuDataCollector::StateOccupancySample* freq_sample);

  // Read CPU frequency data of all CPUs from the file in |path| and fill
  // |cpu_freq_state_names| and |freq_samples| with the data. |cpu_count| is the
  // number of possible CPUs on the system. Note that |freq_samples| is not
  // empty and sample at index i in |freq_samples| corresponds to the freq state
  // information of the i-th CPU. |cpu_online| of each sample must be set before
  // calling this function.
  // Sample file looks like:
  // freq   cpu0    cpu1
  // 126000   223344    223311
  // 216000   93495   93450
  // ...
  // 1800000    321907    331897
  static bool ReadCpuFreqAllTimeInState(
      int cpu_count,
      const base::FilePath& path,
      std::vector<std::string>* cpu_freq_state_names,
      std::vector<CpuDataCollector::StateOccupancySample>* freq_samples);

 private:
  // Posts callbacks to the blocking pool which collect CPU state occupancy
  // samples from the sysfs.
  void PostSampleCpuState();

  // This function commits the CPU count and samples read by
  // SampleCpuStateAsync to |cpu_idle_state_data_| and
  // |cpu_freq_state_data_|. Since UI is the consumer of CPU idle and freq data,
  // this function should run on the UI thread.
  void SaveCpuStateSamplesOnUIThread(
      const int* cpu_count,
      const std::vector<std::string>* cpu_idle_state_names,
      const std::vector<StateOccupancySample>* idle_samples,
      const std::vector<std::string>* cpu_freq_state_names,
      const std::vector<StateOccupancySample>* freq_samples);

  base::RepeatingTimer timer_;

  // Names of the idle states.
  std::vector<std::string> cpu_idle_state_names_;

  // The deque at index <i> in the vector corresponds to the idle state
  // occupancy data of CPU<i>.
  std::vector<StateOccupancySampleDeque> cpu_idle_state_data_;

  // Names of the freq states.
  std::vector<std::string> cpu_freq_state_names_;

  // The deque at index <i> in the vector corresponds to the frequency state
  // occupancy data of CPU<i>.
  std::vector<StateOccupancySampleDeque> cpu_freq_state_data_;

  // The number of CPUs on the system. This value is read from the sysfs, and
  // hence should be read/written to only from the blocking pool.
  int cpu_count_;

  base::WeakPtrFactory<CpuDataCollector> weak_ptr_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(CpuDataCollector);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_CPU_DATA_COLLECTOR_H_
