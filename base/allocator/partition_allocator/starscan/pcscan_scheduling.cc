// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"

#include <algorithm>

namespace base {
namespace internal {

// static
constexpr size_t QuarantineData::kQuarantineSizeMinLimit;

void PCScanScheduler::SetNewSchedulingBackend(
    PCScanSchedulingBackend& backend) {
  backend_ = &backend;
}

// static
constexpr double LimitBackend::kQuarantineSizeFraction;

bool LimitBackend::LimitReached() {
  return true;
}

void LimitBackend::ScanStarted() {
  auto& data = GetQuarantineData();
  data.epoch.fetch_add(1, std::memory_order_relaxed);
  data.last_size = data.current_size.exchange(0, std::memory_order_relaxed);
}

void LimitBackend::UpdateScheduleAfterScan(size_t survived_bytes,
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

}  // namespace internal
}  // namespace base
