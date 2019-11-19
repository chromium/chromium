// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_CHROMEOS_H_
#define BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_CHROMEOS_H_

#include <vector>

#include "base/base_export.h"
#include "base/files/scoped_file.h"
#include "base/macros.h"
#include "base/memory/memory_pressure_listener.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/util/memory_pressure/memory_pressure_voter.h"
#include "base/util/memory_pressure/system_memory_pressure_evaluator.h"

namespace util {
namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// SystemMemoryPressureEvaluator
//
// A class to handle the observation of our free memory. It notifies the
// MemoryPressureListener of memory fill level changes, so that it can take
// action to reduce memory resources accordingly.
class SystemMemoryPressureEvaluator
    : public util::SystemMemoryPressureEvaluator {
 public:
  // The SystemMemoryPressureEvaluator reads the pressure levels from the
  // /sys/kernel/mm/chromeos-low_mem/margin and does not need to be configured.
  //
  // NOTE: You should check that the kernel supports notifications by calling
  // SupportsKernelNotifications() before constructing a new instance of this
  // class.
  explicit SystemMemoryPressureEvaluator(
      std::unique_ptr<MemoryPressureVoter> voter);
  ~SystemMemoryPressureEvaluator() override;

  // GetMarginFileParts returns a vector of the configured margin file values.
  // The margin file contains two or more values, but we're only concerned with
  // the first two. The first represents critical memory pressure, the second
  // is moderate memory pressure level.
  static std::vector<int> GetMarginFileParts();

  // SupportsKernelNotifications will return true if the kernel supports and is
  // configured for notifications on memory availability changes.
  static bool SupportsKernelNotifications();

  // ScheduleEarlyCheck is used by the ChromeOS tab manager delegate to force it
  // to quickly recheck pressure levels after a tab discard or some other
  // action.
  void ScheduleEarlyCheck();

  // Returns the moderate pressure threshold as read from the margin file.
  int ModeratePressureThresholdMBForTesting() const {
    return moderate_pressure_threshold_mb_;
  }

  // Returns the critical pressure threshold as read from the margin file.
  int CriticalPressureThresholdMBForTesting() const {
    return critical_pressure_threshold_mb_;
  }

  // Returns the current system memory pressure evaluator.
  static SystemMemoryPressureEvaluator* Get();

 protected:
  // This constructor is only used for testing.
  SystemMemoryPressureEvaluator(
      const std::string& margin_file,
      const std::string& available_file,
      base::RepeatingCallback<bool(int)> kernel_waiting_callback,
      bool enable_metrics,
      std::unique_ptr<MemoryPressureVoter> voter);

  static std::vector<int> GetMarginFileParts(const std::string& margin_file);
  void CheckMemoryPressure();

 private:
  void HandleKernelNotification(bool result);
  void ScheduleWaitForKernelNotification();
  void CheckMemoryPressureAndRecordStatistics();

  int moderate_pressure_threshold_mb_ = 0;
  int critical_pressure_threshold_mb_ = 0;

  // We keep track of how long it has been since we last notified at the
  // moderate level.
  base::TimeTicks last_moderate_notification_;

  // We keep track of how long it's been since we notified on the
  // Memory.PressureLevel metric.
  base::TimeTicks last_pressure_level_report_;

  // File descriptor used to read and poll(2) available memory from sysfs,
  // In /sys/kernel/mm/chromeos-low_mem/available.
  base::ScopedFD available_mem_file_;

  // A periodic timer which will be used to report a UMA metric on the current
  // memory pressure level as theoretically we could go a very long time without
  // ever receiving a notification.
  base::RepeatingTimer reporting_timer_;

  // Kernel waiting callback which is responsible for blocking on the
  // available file until it receives a kernel notification, this is
  // configurable to make testing easier.
  base::RepeatingCallback<bool()> kernel_waiting_callback_;

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<SystemMemoryPressureEvaluator> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(SystemMemoryPressureEvaluator);
};

}  // namespace chromeos
}  // namespace util
#endif  // BASE_UTIL_MEMORY_PRESSURE_SYSTEM_MEMORY_PRESSURE_EVALUATOR_CHROMEOS_H_
