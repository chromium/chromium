// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

namespace {
constexpr size_t kMB = 1024 * 1024;
}  // namespace

TEST(PCScanSchedulerLimitBackendTest, NoScanBelowMinimumScanningThreshold) {
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

TEST(PCScanSchedulerLimitBackendTest, ScanAtQuarantineSizeFraction) {
  PCScanScheduler scheduler;
  LimitBackend limit_backend(scheduler);
  scheduler.SetNewSchedulingBackend(limit_backend);
  constexpr size_t kHeapSize = 100 * kMB;
  limit_backend.GrowLimitIfNeeded(kHeapSize);
  constexpr size_t kExpectedTriggerSize = static_cast<size_t>(
      static_cast<double>(kHeapSize) * LimitBackend::kQuarantineSizeFraction);
  EXPECT_FALSE(scheduler.AccountFreed(kExpectedTriggerSize / 2));
  EXPECT_FALSE(
      scheduler.AccountFreed(kExpectedTriggerSize - kExpectedTriggerSize / 2));
  EXPECT_TRUE(scheduler.AccountFreed(1));
}

}  // namespace internal
}  // namespace base