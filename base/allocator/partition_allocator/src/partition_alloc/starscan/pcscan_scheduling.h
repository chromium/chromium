// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STARSCAN_PCSCAN_SCHEDULING_H_
#define PARTITION_ALLOC_STARSCAN_PCSCAN_SCHEDULING_H_

#include <atomic>
#include <cstdint>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc::internal {

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

// No virtual destructor to allow constant initialization of PCScan as
// static global which directly embeds LimitBackend as default backend.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnon-virtual-dtor"
#endif
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PCScanSchedulingBackend {
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

 public:
  inline constexpr explicit PCScanSchedulingBackend(PCScanScheduler&);

  PCScanSchedulingBackend(const PCScanSchedulingBackend&) = delete;
  PCScanSchedulingBackend& operator=(const PCScanSchedulingBackend&) = delete;

  void DisableScheduling();
  void EnableScheduling();

  bool is_scheduling_enabled() const {
    return scheduling_enabled_.load(std::memory_order_relaxed);
  }

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
  virtual base::TimeDelta UpdateDelayedSchedule();

 protected:
  inline bool SchedulingDisabled() const;

  virtual bool NeedsToImmediatelyScan() = 0;

  PCScanScheduler& scheduler_;
  std::atomic<bool> scheduling_enabled_{true};
};

// Scheduling backend that just considers a single hard limit.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) LimitBackend final
    : public PCScanSchedulingBackend {
 public:
  static constexpr double kQuarantineSizeFraction = 0.1;

  inline constexpr explicit LimitBackend(PCScanScheduler&);

  bool LimitReached() final;
  void UpdateScheduleAfterScan(size_t, base::TimeDelta, size_t) final;

 private:
  bool NeedsToImmediatelyScan() final;
};

// Task based backend that is aware of a target mutator utilization that
// specifies how much percent of the execution should be reserved for the
// mutator. I.e., the MU-aware scheduler ensures that scans are limit and
// there is enough time left for the mutator to execute the actual application
// workload.
//
// See constants below for trigger mechanisms.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) MUAwareTaskBasedBackend final
    : public PCScanSchedulingBackend {
 public:
  using ScheduleDelayedScanFunc = void (*)(int64_t delay_in_microseconds);

  MUAwareTaskBasedBackend(PCScanScheduler&, ScheduleDelayedScanFunc);
  ~MUAwareTaskBasedBackend();

  bool LimitReached() final;
  size_t ScanStarted() final;
  void UpdateScheduleAfterScan(size_t, base::TimeDelta, size_t) final;
  base::TimeDelta UpdateDelayedSchedule() final;

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

  bool NeedsToImmediatelyScan() final;

  // Callback to schedule a delayed scan.
  const ScheduleDelayedScanFunc schedule_delayed_scan_;

  Lock scheduler_lock_;
  size_t hard_limit_ PA_GUARDED_BY(scheduler_lock_){0};
  base::TimeTicks earliest_next_scan_time_ PA_GUARDED_BY(scheduler_lock_);

  friend class PartitionAllocPCScanMUAwareTaskBasedBackendTest;
};

// The scheduler that is embedded in the PCSCan frontend which requires a fast
// path for freeing objects. The scheduler holds data needed to invoke a
// `PCScanSchedulingBackend` upon hitting a limit. The backend implements
// the actual scheduling strategy and is in charge of maintaining limits.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PCScanScheduler final {
 public:
  inline constexpr PCScanScheduler();

  PCScanScheduler(const PCScanScheduler&) = delete;
  PCScanScheduler& operator=(const PCScanScheduler&) = delete;

  // Account freed `bytes`. Returns true if scan should be triggered
  // immediately, and false otherwise.
  PA_ALWAYS_INLINE bool AccountFreed(size_t bytes);

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

PA_ALWAYS_INLINE bool PCScanScheduler::AccountFreed(size_t size) {
  const size_t size_before =
      quarantine_data_.current_size.fetch_add(size, std::memory_order_relaxed);
  return (size_before + size >
          quarantine_data_.size_limit.load(std::memory_order_relaxed)) &&
         backend_->LimitReached();
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_STARSCAN_PCSCAN_SCHEDULING_H_
