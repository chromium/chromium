// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_UTILS_H_
#define BASE_MEMORY_COORDINATOR_UTILS_H_

#include "base/memory_coordinator/memory_consumer.h"

namespace base {

// These constants represent memory limit thresholds (expressed as a percentage)
// that correspond to legacy memory pressure levels. They are intended to assist
// with the migration of clients from MemoryPressureListener to MemoryConsumer.
inline constexpr int kNoMemoryPressureThreshold =
    MemoryConsumer::kDefaultMemoryLimit;
inline constexpr int kModerateMemoryPressureThreshold =
    kNoMemoryPressureThreshold / 2;
inline constexpr int kCriticalMemoryPressureThreshold = 0;

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_UTILS_H_
