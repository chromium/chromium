// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"

#include "base/synchronization/lock.h"
#include "base/test/bind.h"
#include "base/time/time.h"
#include "base/time/time_override.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {
constexpr size_t kMB = 1024 * 1024;
}  // namespace

TEST(PartitionAllocPCScanSchedulerLimitBackendTest,
     NoScanBelowMinimumScanningThreshold) {
  PCScanScheduler scheduler;
  LimitBackend limit_backend(scheduler);
  scheduler.SetNewSchedulingBackend(limit_backend);
  constexpr size_t kMinimumScanningThreshold =
      QuarantineData::kQuarantineSizeMinLimit;
  EXPECT_FALSE(scheduler.AccountFreed(kMinimumScanningThreshold / 2));
  EXPECT_FALSE(scheduler.AccountFreed(kMinimumScanningThreshold -
                                      kMinimumScanningThreshold / 2));
  EXPECT_TRUE(scheduler.AccountFreed(1));
}

TEST(PartitionAllocPCScanSchedulerLimitBackendTest,
     ScanAtQuarantineSizeFraction) {
  PCScanScheduler scheduler;
  LimitBackend limit_backend(scheduler);
  scheduler.SetNewSchedulingBackend(limit_backend);
  constexpr size_t kHeapSize = 100 * kMB;
  constexpr size_t kNoSurvivedBytes = 0;
  limit_backend.UpdateScheduleAfterScan(kNoSurvivedBytes, base::TimeDelta(),
                                        kHeapSize);
  constexpr size_t kExpectedTriggerSize = static_cast<size_t>(
      static_cast<double>(kHeapSize) * LimitBackend::kQuarantineSizeFraction);
  EXPECT_FALSE(scheduler.AccountFreed(kExpectedTriggerSize / 2));
  EXPECT_FALSE(
      scheduler.AccountFreed(kExpectedTriggerSize - kExpectedTriggerSize / 2));
  EXPECT_TRUE(scheduler.AccountFreed(1));
}

class PartitionAllocPCScanMUAwareTaskBasedBackendTest : public ::testing::Test {
 public:
  static constexpr size_t kHeapSize = 100 * kMB;

  static constexpr size_t HardLimitSize(size_t heap_size) {
    return static_cast<size_t>(
               static_cast<double>(heap_size) *
               MUAwareTaskBasedBackend::kHardLimitQuarantineSizePercent) +
           1;
  }

  static constexpr size_t SoftLimitSize(size_t heap_size) {
    return static_cast<size_t>(
               static_cast<double>(heap_size) *
               MUAwareTaskBasedBackend::kSoftLimitQuarantineSizePercent) +
           1;
  }

  PartitionAllocPCScanMUAwareTaskBasedBackendTest()
      : backend_(scheduler_,
                 base::BindLambdaForTesting([this](TimeDelta delay) {
                   delayed_scan_scheduled_count_++;
                 })) {
    scheduler_.SetNewSchedulingBackend(backend_);
    constexpr size_t kNoSurvivedBytes = 0;
    constexpr base::TimeDelta kZeroTimeForScan;
    backend_.UpdateScheduleAfterScan(kNoSurvivedBytes, kZeroTimeForScan,
                                     kHeapSize);
  }

  PCScanScheduler& scheduler() { return scheduler_; }
  MUAwareTaskBasedBackend& backend() { return backend_; }
  size_t delayed_scan_scheduled_count() const {
    return delayed_scan_scheduled_count_;
  }

 private:
  PCScanScheduler scheduler_;
  MUAwareTaskBasedBackend backend_;
  size_t delayed_scan_scheduled_count_{0};
};

namespace {

class ScopedTimeTicksOverride final {
 public:
  ScopedTimeTicksOverride()
      : ScopedTimeTicksOverride(InitializeTimeAndReturnTimeTicksNow()) {}

  void AddTicksToNow(TimeDelta ticks) { now_ticks_ += ticks; }

 private:
  static TimeTicks Now() { return now_ticks_; }

  static TimeTicksNowFunction InitializeTimeAndReturnTimeTicksNow() {
    now_ticks_ = TimeTicks::Now();
    return &Now;
  }

  explicit ScopedTimeTicksOverride(TimeTicksNowFunction time_ticks_function)
      : overrides_(nullptr, time_ticks_function, nullptr) {}

  static TimeTicks now_ticks_;

  subtle::ScopedTimeClockOverrides overrides_;
};

// static
TimeTicks ScopedTimeTicksOverride::now_ticks_;

}  // namespace

TEST_F(PartitionAllocPCScanMUAwareTaskBasedBackendTest,
       SoftLimitSchedulesScanIfMUNotSatisfied) {
  // Stop the time.
  ScopedTimeTicksOverride now_ticks_override;
  // Simulate PCScan that processed kHeapSize in 1s. Since time is stopped that
  // schedule is not reachable.
  backend().UpdateScheduleAfterScan(0, TimeDelta::FromSeconds(1), kHeapSize);

  EXPECT_EQ(0u, delayed_scan_scheduled_count());
  EXPECT_FALSE(scheduler().AccountFreed(SoftLimitSize(kHeapSize)));
  EXPECT_EQ(1u, delayed_scan_scheduled_count());
}

TEST_F(PartitionAllocPCScanMUAwareTaskBasedBackendTest,
       SoftLimitInvokesScanIfMUSatisfied) {
  // Stop the time.
  ScopedTimeTicksOverride now_ticks_override;
  // Simulate PCScan that processed kHeapSize in 0s. The next scan should thus
  // happen immediately.
  backend().UpdateScheduleAfterScan(0, TimeDelta::FromSeconds(0), kHeapSize);

  EXPECT_EQ(0u, delayed_scan_scheduled_count());
  EXPECT_TRUE(scheduler().AccountFreed(SoftLimitSize(kHeapSize)));
  EXPECT_EQ(0u, delayed_scan_scheduled_count());
}

TEST_F(PartitionAllocPCScanMUAwareTaskBasedBackendTest,
       HardLimitSchedulesScanImmediately) {
  // Stop the time.
  ScopedTimeTicksOverride now_ticks_override;
  // Simulate PCScan that processed kHeapSize in 1s. Since time is stopped that
  // schedule is not reachable.
  backend().UpdateScheduleAfterScan(0, TimeDelta::FromSeconds(0), kHeapSize);

  EXPECT_EQ(0u, delayed_scan_scheduled_count());
  // Triogering the hard limit should immediately require a scan and not
  // schedule anything.
  EXPECT_TRUE(scheduler().AccountFreed(HardLimitSize(kHeapSize)));
  EXPECT_EQ(0u, delayed_scan_scheduled_count());
}

}  // namespace internal
}  // namespace base
