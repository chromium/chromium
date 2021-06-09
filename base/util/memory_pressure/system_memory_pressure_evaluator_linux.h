// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_LINUX_H_
#define BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_LINUX_H_

#include "base/memory/memory_pressure_listener.h"
#include "base/process/process_metrics.h"
#include "base/sequence_checker.h"
#include "base/timer/timer.h"
#include "base/util/memory_pressure/memory_pressure_voter.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace util {
namespace os_linux {

// Linux memory pressure voter. Because there is no OS provided signal this
// polls at a low frequency, and applies internal hysteresis.
// TODO(https://crbug.com/1119396): use Pressure Stall Information (PSI) on
// kernels >4.20.
class SystemMemoryPressureEvaluator
    : public util::SystemMemoryPressureEvaluator {
 public:
  using MemoryPressureLevel = base::MemoryPressureListener::MemoryPressureLevel;

  // The memory sampling period, currently 5s.
  static const base::TimeDelta kMemorySamplingPeriod;

  // Constants governing the polling and hysteresis behaviour of the observer.
  // The time which should pass between 2 successive moderate memory pressure
  // signals.
  static const base::TimeDelta kModeratePressureCooldown;

  // Default minimum free memory thresholds, in percents.
  static const int kDefaultModerateThresholdPc;
  static const int kDefaultCriticalThresholdPc;

  // Default constructor. Will choose thresholds automatically based on the
  // actual amount of system memory.
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);

  // Constructor with explicit memory thresholds. These represent the amount of
  // free memory below which the applicable memory pressure state engages.
  SystemMemoryPressureEvaluator(int moderate_threshold_mb,
                                int critical_threshold_mb,
                                std::unique_ptr<MemoryPressureVoter> voter);

  ~SystemMemoryPressureEvaluator() override = default;

  SystemMemoryPressureEvaluator(const SystemMemoryPressureEvaluator&) = delete;
  SystemMemoryPressureEvaluator& operator=(
      const SystemMemoryPressureEvaluator&) = delete;

  // Returns the moderate pressure level free memory threshold, in MB.
  int moderate_threshold_mb() const { return moderate_threshold_mb_; }

  // Returns the critical pressure level free memory threshold, in MB.
  int critical_threshold_mb() const { return critical_threshold_mb_; }

 protected:
  // Internals are exposed for unittests.

  // Starts observing the memory fill level. Calls to StartObserving should
  // always be matched with calls to StopObserving.
  void StartObserving();

  // Stop observing the memory fill level. May be safely called if
  // StartObserving has not been called. Must be called from the same thread on
  // which the monitor was instantiated.
  void StopObserving();

  // Checks memory pressure, storing the current level, applying any hysteresis
  // and emitting memory pressure level change signals as necessary. This
  // function is called periodically while the monitor is observing memory
  // pressure. Must be called from the same thread on which the monitor was
  // instantiated.
  void CheckMemoryPressure();

  // Automatically infers threshold values based on system memory.
  // Returns 'true' if succeeded.
  bool InferThresholds();

  // Calculates the current instantaneous memory pressure level. This does not
  // use any hysteresis and simply returns the result at the current moment. Can
  // be called on any thread.
  MemoryPressureLevel CalculateCurrentPressureLevel();

  // This is just a wrapper for base:: function;
  // declared as virtual for unit testing
  virtual bool GetSystemMemoryInfo(base::SystemMemoryInfoKB* mem_info);

 private:
  // Threshold amounts of available memory that trigger pressure levels
  int moderate_threshold_mb_;
  int critical_threshold_mb_;

  // A periodic timer to check for memory pressure changes.
  base::RepeatingTimer timer_;

  // To slow down the amount of moderate pressure event calls, this gets used to
  // count the number of events since the last event occurred. This is used by
  // |CheckMemoryPressure| to apply hysteresis on the raw results of
  // |CalculateCurrentPressureLevel|.
  int moderate_pressure_repeat_count_;

  // Ensures that this object is used from a single sequence.
  SEQUENCE_CHECKER(sequence_checker_);
};

}  // namespace os_linux
}  // namespace util

#endif  // BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_LINUX_H_
