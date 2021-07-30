// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_

#include <atomic>
#include <cstdint>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/synchronization/lock.h"
#include "base/time/time.h"

namespace base {
namespace internal {

class PCScanScheduler;

struct QuarantineData final {
  static constexpr size_t kQuarantineSizeMinLimit = 1 * 1024 * 1024;

  inline constexpr QuarantineData();

  bool MinimumScanningThresholdReached() const {
    return current_size.load(std::memory_order_relaxed) >
           kQuarantineSizeMinLimit;
  }

  std::atomic<size_t> current_size{0u};
  std::atomic<size_t> size_limit{kQuarantineSizeMinLimit};
  std::atomic<size_t> epoch{0u};
};

class BASE_EXPORT PCScanSchedulingBackend {
 public:
  explicit inline constexpr PCScanSchedulingBackend(PCScanScheduler&);
  // No virtual destructor to allow constant initialization of PCScan as
  // static global which directly embeds LimitBackend as default backend.

  PCScanSchedulingBackend(const PCScanSchedulingBackend&) = delete;
  PCScanSchedulingBackend& operator=(const PCScanSchedulingBackend&) = delete;

  inline QuarantineData& GetQuarantineData();

  // Invoked when the limit in PCScanScheduler is reached. Returning true
  // signals the caller to invoke a scan.
  virtual bool LimitReached() = 0;

  // Invoked on starting a scan. Returns current quarantine size.
  virtual size_t ScanStarted();

  // Invoked at the end of a scan to compute a new limit.
  virtual void UpdateScheduleAfterScan(size_t survived_bytes,
                                       base::TimeDelta time_spent_in_scan,
                                       size_t heap_size) = 0;

  // Invoked by PCScan to ask for a new timeout for a scheduled PCScan task.
  // Only invoked if scheduler requests a delayed scan at some point.
  virtual TimeDelta UpdateDelayedSchedule();

 protected:
  PCScanScheduler& scheduler_;
};

// Scheduling backend that just considers a single hard limit.
class BASE_EXPORT LimitBackend final : public PCScanSchedulingBackend {
 public:
  static constexpr double kQuarantineSizeFraction = 0.1;

  explicit inline constexpr LimitBackend(PCScanScheduler&);

  bool LimitReached() final;
  void UpdateScheduleAfterScan(size_t, base::TimeDelta, size_t) final;
};

// Task based backend that is aware of a target mutator utilization that
// specifies how much percent of the execution should be reserved for the
// mutator. I.e., the MU-aware scheduler ensures that scans are limit and
// there is enough time left for the mutator to execute the actual application
// workload.
//
// See constants below for trigger mechanisms.
class BASE_EXPORT MUAwareTaskBasedBackend final
    : public PCScanSchedulingBackend {
 public:
  MUAwareTaskBasedBackend(PCScanScheduler&,
                          base::RepeatingCallback<void(TimeDelta)>);
  ~MUAwareTaskBasedBackend();

  bool LimitReached() final;
  size_t ScanStarted() final;
  void UpdateScheduleAfterScan(size_t, base::TimeDelta, size_t) final;
  TimeDelta UpdateDelayedSchedule() final;

 private:
  // Limit triggering the scheduler. If `kTargetMutatorUtilizationPercent` is
  // satisfied at this point then a scan is triggered immediately.
  static constexpr double kSoftLimitQuarantineSizePercent = 0.1;
  // Hard limit at which a scan is triggered in any case. Avoids blowing up the
  // heap completely.
  static constexpr double kHardLimitQuarantineSizePercent = 0.5;
  // Target mutator utilization that is respected when invoking a scan.
  // Specifies how much percent of walltime should be spent in the mutator.
  // Inversely, specifies how much walltime (indirectly CPU) is spent on
  // memory management in scan.
  static constexpr double kTargetMutatorUtilizationPercent = 0.90;

  // Callback to schedule a delayed scan.
  const base::RepeatingCallback<void(TimeDelta)> schedule_delayed_scan_;

  base::Lock scheduler_lock_;
  size_t hard_limit_ GUARDED_BY(scheduler_lock_){0};
  base::TimeTicks earliest_next_scan_time_ GUARDED_BY(scheduler_lock_);

  friend class PartitionAllocPCScanMUAwareTaskBasedBackendTest;
};

// The scheduler that is embedded in the PCSCan frontend which requires a fast
// path for freeing objects. The scheduler holds data needed to invoke a
// `PCScanSchedulingBackend` upon hitting a limit. The backend implements
// the actual scheduling strategy and is in charge of maintaining limits.
class BASE_EXPORT PCScanScheduler final {
 public:
  inline constexpr PCScanScheduler();

  PCScanScheduler(const PCScanScheduler&) = delete;
  PCScanScheduler& operator=(const PCScanScheduler&) = delete;

  // Account freed `bytes`. Returns true if scan should be triggered
  // immediately, and false otherwise.
  ALWAYS_INLINE bool AccountFreed(size_t bytes);

  size_t epoch() const {
    return quarantine_data_.epoch.load(std::memory_order_relaxed);
  }

  // Sets a new scheduling backend that should be used by the scanner.
  void SetNewSchedulingBackend(PCScanSchedulingBackend&);

  PCScanSchedulingBackend& scheduling_backend() { return *backend_; }
  const PCScanSchedulingBackend& scheduling_backend() const {
    return *backend_;
  }

 private:
  QuarantineData quarantine_data_{};
  // The default backend used is a simple LimitBackend that just triggers scan
  // on reaching a hard limit.
  LimitBackend default_scheduling_backend_{*this};
  PCScanSchedulingBackend* backend_ = &default_scheduling_backend_;

  friend PCScanSchedulingBackend;
};

// To please Chromium's clang plugin.
constexpr PCScanScheduler::PCScanScheduler() = default;
constexpr QuarantineData::QuarantineData() = default;

constexpr PCScanSchedulingBackend::PCScanSchedulingBackend(
    PCScanScheduler& scheduler)
    : scheduler_(scheduler) {}

QuarantineData& PCScanSchedulingBackend::GetQuarantineData() {
  return scheduler_.quarantine_data_;
}

constexpr LimitBackend::LimitBackend(PCScanScheduler& scheduler)
    : PCScanSchedulingBackend(scheduler) {}

bool PCScanScheduler::AccountFreed(size_t size) {
  const size_t size_before =
      quarantine_data_.current_size.fetch_add(size, std::memory_order_relaxed);
  return (size_before + size >
          quarantine_data_.size_limit.load(std::memory_order_relaxed)) &&
         backend_->LimitReached();
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_SCHEDULING_H_
