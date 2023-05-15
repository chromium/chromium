// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"

#include <algorithm>
#include <atomic>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/time/time.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/starscan/logging.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"

namespace partition_alloc::internal {

// static
constexpr size_t QuarantineData::kQuarantineSizeMinLimit;

void PCScanScheduler::SetNewSchedulingBackend(
    PCScanSchedulingBackend& backend) {
  backend_ = &backend;
}

void PCScanSchedulingBackend::DisableScheduling() {
  scheduling_enabled_.store(false, std::memory_order_relaxed);
}

void PCScanSchedulingBackend::EnableScheduling() {
  scheduling_enabled_.store(true, std::memory_order_relaxed);
  // Check if *Scan needs to be run immediately.
  if (NeedsToImmediatelyScan()) {
    PCScan::PerformScan(PCScan::InvocationMode::kNonBlocking);
  }
}

size_t PCScanSchedulingBackend::ScanStarted() {
  auto& data = GetQuarantineData();
  data.epoch.fetch_add(1, std::memory_order_relaxed);
  return data.current_size.exchange(0, std::memory_order_relaxed);
}

base::TimeDelta PCScanSchedulingBackend::UpdateDelayedSchedule() {
  return base::TimeDelta();
}

// static
constexpr double LimitBackend::kQuarantineSizeFraction;

bool LimitBackend::LimitReached() {
  return is_scheduling_enabled();
}

void LimitBackend::UpdateScheduleAfterScan(size_t survived_bytes,
                                           base::TimeDelta,
                                           size_t heap_size) {
  scheduler_.AccountFreed(survived_bytes);
  // |heap_size| includes the current quarantine size, we intentionally leave
  // some slack till hitting the limit.
  auto& data = GetQuarantineData();
  data.size_limit.store(
      std::max(QuarantineData::kQuarantineSizeMinLimit,
               static_cast<size_t>(kQuarantineSizeFraction * heap_size)),
      std::memory_order_relaxed);
}

bool LimitBackend::NeedsToImmediatelyScan() {
  return false;
}

// static
constexpr double MUAwareTaskBasedBackend::kSoftLimitQuarantineSizePercent;
// static
constexpr double MUAwareTaskBasedBackend::kHardLimitQuarantineSizePercent;
// static
constexpr double MUAwareTaskBasedBackend::kTargetMutatorUtilizationPercent;

MUAwareTaskBasedBackend::MUAwareTaskBasedBackend(
    PCScanScheduler& scheduler,
    ScheduleDelayedScanFunc schedule_delayed_scan)
    : PCScanSchedulingBackend(scheduler),
      schedule_delayed_scan_(schedule_delayed_scan) {
  PA_DCHECK(schedule_delayed_scan_);
}

MUAwareTaskBasedBackend::~MUAwareTaskBasedBackend() = default;

bool MUAwareTaskBasedBackend::LimitReached() {
  bool should_reschedule = false;
  base::TimeDelta reschedule_delay;
  {
    ScopedGuard guard(scheduler_lock_);
    // At this point we reached a limit where the schedule generally wants to
    // trigger a scan.
    if (hard_limit_) {
      // The hard limit is not reset, indicating that the scheduler only hit the
      // soft limit. See inlined comments for the algorithm.
      auto& data = GetQuarantineData();
      PA_DCHECK(hard_limit_ >= QuarantineData::kQuarantineSizeMinLimit);
      // 1. Update the limit to the hard limit which will always immediately
      // trigger a scan.
      data.size_limit.store(hard_limit_, std::memory_order_relaxed);
      hard_limit_ = 0;

      // 2. Unlikely case: If also above hard limit, start scan right away. This
      // ignores explicit PCScan disabling.
      if (PA_UNLIKELY(data.current_size.load(std::memory_order_relaxed) >
                      data.size_limit.load(std::memory_order_relaxed))) {
        return true;
      }

      // 3. Check if PCScan was explicitly disabled.
      if (PA_UNLIKELY(!is_scheduling_enabled())) {
        return false;
      }

      // 4. Otherwise, the soft limit would trigger a scan immediately if the
      // mutator utilization requirement is satisfied.
      reschedule_delay = earliest_next_scan_time_ - base::TimeTicks::Now();
      if (reschedule_delay <= base::TimeDelta()) {
        // May invoke scan immediately.
        return true;
      }

      PA_PCSCAN_VLOG(3) << "Rescheduling scan with delay: "
                        << reschedule_delay.InMillisecondsF() << " ms";
      // 5. If the MU requirement is not satisfied, schedule a delayed scan to
      // the time instance when MU is satisfied.
      should_reschedule = true;
    }
  }
  // Don't reschedule under the lock as the callback can call free() and
  // recursively enter the lock.
  if (should_reschedule) {
    schedule_delayed_scan_(reschedule_delay.InMicroseconds());
    return false;
  }
  return true;
}

size_t MUAwareTaskBasedBackend::ScanStarted() {
  ScopedGuard guard(scheduler_lock_);

  return PCScanSchedulingBackend::ScanStarted();
}

void MUAwareTaskBasedBackend::UpdateScheduleAfterScan(
    size_t survived_bytes,
    base::TimeDelta time_spent_in_scan,
    size_t heap_size) {
  scheduler_.AccountFreed(survived_bytes);

  ScopedGuard guard(scheduler_lock_);

  // |heap_size| includes the current quarantine size, we intentionally leave
  // some slack till hitting the limit.
  auto& data = GetQuarantineData();
  data.size_limit.store(
      std::max(
          QuarantineData::kQuarantineSizeMinLimit,
          static_cast<size_t>(kSoftLimitQuarantineSizePercent * heap_size)),
      std::memory_order_relaxed);
  hard_limit_ = std::max(
      QuarantineData::kQuarantineSizeMinLimit,
      static_cast<size_t>(kHardLimitQuarantineSizePercent * heap_size));

  // This computes the time window that the scheduler will reserve for the
  // mutator. Scanning, unless reaching the hard limit, will generally be
  // delayed until this time has passed.
  const auto time_required_on_mutator =
      time_spent_in_scan * kTargetMutatorUtilizationPercent /
      (1.0 - kTargetMutatorUtilizationPercent);
  earliest_next_scan_time_ = base::TimeTicks::Now() + time_required_on_mutator;
}

bool MUAwareTaskBasedBackend::NeedsToImmediatelyScan() {
  bool should_reschedule = false;
  base::TimeDelta reschedule_delay;
  {
    ScopedGuard guard(scheduler_lock_);
    // If |hard_limit_| was set to zero, the soft limit was reached. Bail out if
    // it's not.
    if (hard_limit_) {
      return false;
    }

    // Check if mutator utilization requiremet is satisfied.
    reschedule_delay = earliest_next_scan_time_ - base::TimeTicks::Now();
    if (reschedule_delay <= base::TimeDelta()) {
      // May invoke scan immediately.
      return true;
    }

    PA_PCSCAN_VLOG(3) << "Rescheduling scan with delay: "
                      << reschedule_delay.InMillisecondsF() << " ms";
    // Schedule a delayed scan to the time instance when MU is satisfied.
    should_reschedule = true;
  }
  // Don't reschedule under the lock as the callback can call free() and
  // recursively enter the lock.
  if (should_reschedule) {
    schedule_delayed_scan_(reschedule_delay.InMicroseconds());
  }
  return false;
}

base::TimeDelta MUAwareTaskBasedBackend::UpdateDelayedSchedule() {
  ScopedGuard guard(scheduler_lock_);
  // TODO(1197479): Adjust schedule to current heap sizing.
  const auto delay = earliest_next_scan_time_ - base::TimeTicks::Now();
  PA_PCSCAN_VLOG(3) << "Schedule is off by " << delay.InMillisecondsF() << "ms";
  return delay >= base::TimeDelta() ? delay : base::TimeDelta();
}

}  // namespace partition_alloc::internal
