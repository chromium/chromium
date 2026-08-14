// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_COORDINATOR_UTILS_H_
#define BASE_MEMORY_COORDINATOR_UTILS_H_

#include "base/memory_coordinator/memory_limit.h"

namespace base {

// These constants represent memory limit thresholds that correspond to legacy
// memory pressure levels. They are intended to assist with the migration of
// clients from MemoryPressureListener to MemoryConsumer.
//
// Deprecated: Use `base::MemoryLimit::Default()`,
// `base::MemoryLimit::ModeratePressureThreshold()`, or
// `base::MemoryLimit::CriticalPressureThreshold()` directly.
// TODO(crbug.com/441951621): Remove after migration to base::MemoryLimit is
// complete.
inline constexpr MemoryLimit kNoMemoryPressureThreshold =
    MemoryLimit::NoPressureThreshold();
inline constexpr MemoryLimit kModerateMemoryPressureThreshold =
    MemoryLimit::ModeratePressureThreshold();
inline constexpr MemoryLimit kCriticalMemoryPressureThreshold =
    MemoryLimit::CriticalPressureThreshold();

}  // namespace base

#endif  // BASE_MEMORY_COORDINATOR_UTILS_H_
