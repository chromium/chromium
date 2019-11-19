// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_
#define CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_

#include <deque>
#include <utility>

#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"

namespace base {
class TickClock;
}

namespace memory {
namespace internal {

// An observation window consists of a series of samples. Samples in this window
// will have a maximum age (the window length), samples that exceed that age
// will be automatically trimmed when a new one is available.
//
// Concurrent calls to instances of this class aren't allowed.
template <typename T>
class ObservationWindow {
 public:
  explicit ObservationWindow(const base::TimeDelta window_length);
  virtual ~ObservationWindow();

  // Function that should be called each time a new sample is available. When a
  // new sample gets added the entries that exceeds the age of this window will
  // first be removed, and then the new one will be added to it.
  void OnSample(const T sample);

  // Returns the number of samples in this window.
  size_t SampleCount() const { return observations_.size(); }

 protected:
  using Observation = std::pair<const base::TimeTicks, const T>;

  // Called each time a sample gets added to the observation window. This gets
  // called before inserting the sample into the observation window.
  virtual void OnSampleAdded(const T& sample) = 0;

  // Called each time a sample gets removed from the observation window. This
  // gets called before removing the sample from the observation window.
  virtual void OnSampleRemoved(const T& sample) = 0;

  const std::deque<Observation>& observations_for_testing() {
    return observations_;
  }

  void set_clock_for_testing(const base::TickClock* clock) { clock_ = clock; }

 private:
  // The length of the window. Samples older than |base::TimeTicks::Now() -
  // window_length_| will automatically be removed from this window when a new
  // one gets added.
  const base::TimeDelta window_length_;

  // The observations, in order of arrival.
  std::deque<Observation> observations_;

  // Allow for an injectable clock for testing.
  const base::TickClock* clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(ObservationWindow);
};

}  // namespace internal

// Implementation of an observation window that can be used to track the amount
// of free physical memory over time. The user is responsible for providing the
// samples via the |OnSample| method.
class FreeMemoryObservationWindow : public internal::ObservationWindow<int> {
 public:
  // Configuration of the observation window. The user is responsible for
  // providing some appropriate values depending on how it implements the
  // memory pressure detector. The values provided below are simple reasonable
  // approximations of what value could be used here, they're not based on any
  // metric.
  struct Config {
    // The minimum number of samples to consider that the data in this window is
    // meaningful.
    const size_t min_sample_count = 4;

    // The ratio of samples that have to be below one of the thresholds to
    // consider that the amount of free memory is really below this value.
    const float sample_ratio_to_be_positive = 0.75;

    // Default value for the different thresholds, the users of this class will
    // want to provide a more appropriate value here (based on the platform
    // specs). The early limit is used to indicate that the memory is getting
    // low and that more metrics should probably be tracked. The critical limit
    // indicates that the system is low on memory and that performance will
    // suffer.
    const int low_memory_early_limit_mb = 600;
    const int low_memory_critical_limit_mb = 400;
  };

  FreeMemoryObservationWindow(const base::TimeDelta window_length,
                              const Config& config);
  ~FreeMemoryObservationWindow() override;

  // Check if the memory is under one of the limits.
  bool MemoryIsUnderEarlyLimit() const;
  bool MemoryIsUnderCriticalLimit() const;

  const Config& config_for_testing() const { return config_; }

 private:
  FRIEND_TEST_ALL_PREFIXES(ObservationWindowTest, FreeMemoryObservationWindow);

  // internal::ObservationWindow:
  void OnSampleAdded(const int& sample) override;
  void OnSampleRemoved(const int& sample) override;

  bool MemoryIsUnderLimitImpl(size_t sample_under_limit_cnt) const;

  // The current number of samples that are below each threshold.
  size_t sample_below_early_limit_count_ = 0;
  size_t sample_below_critical_limit_count_ = 0;

  Config config_ = {};

  DISALLOW_COPY_AND_ASSIGN(FreeMemoryObservationWindow);
};

// Implementation of an observation window that can be used to track the disk
// idle time over time. The user is responsible for providing the samples via
// the |OnSample| method, these samples should represent the percentage of time
// the disk has been idle since the last observation and should have a value in
// the [0.0, 1.0] range.
class DiskIdleTimeObservationWindow
    : public internal::ObservationWindow<float> {
 public:
  // This window computes the average disk idle time value over time, so it
  // should have a relatively short length to ensure that it doesn't take too
  // many new samples to move the average.
  // |threshold| is the limit that will be used to determine that the disk idle
  // time is low, should have a value in the [0, 1.0] range.
  //
  // TODO(sebmarchand): Use an exponential moving average instead of a simple
  // average, the age of the sample could be used to compute its weight.
  DiskIdleTimeObservationWindow(const base::TimeDelta window_length,
                                const float threshold);
  ~DiskIdleTimeObservationWindow() override;

  // Check if the disk idle time was low over the observation period.
  bool DiskIdleTimeIsLow() const;

 private:
  FRIEND_TEST_ALL_PREFIXES(ObservationWindowTest,
                           DiskIdleTimeObservationWindow);

  // internal::ObservationWindow:
  void OnSampleAdded(const float& sample) override;
  void OnSampleRemoved(const float& sample) override;

  // The current sum of all the samples.
  float sum_ = 0.0;

  // The threshold under which the disk idle time will be considered as low, in
  // the [0, 1] range.
  const float threshold_;

  DISALLOW_COPY_AND_ASSIGN(DiskIdleTimeObservationWindow);
};

}  // namespace memory

#include "chrome/browser/memory/memory_pressure_monitor_utils_impl.h"

#endif  // CHROME_BROWSER_MEMORY_MEMORY_PRESSURE_MONITOR_UTILS_H_
