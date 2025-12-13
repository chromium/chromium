// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_CONFIG_H_
#define BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_CONFIG_H_

#include <string>

#include "base/base_export.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"

// This header declares utilities to load
// `::partition_alloc::internal::SchedulerLoopQuarantineConfig` for the current
// process from the feature list.
namespace base::allocator {

enum class SchedulerLoopQuarantineBranchType {
  // The global quarantine branch, shared across threads.
  kGlobal,
  // Default configuration for thread-local branches on new threads.
  kThreadLocalDefault,
  // Specialized configuration for the main thread of a process.
  kMain,
  // Specialized configuration for the IO thread of a process.
  kIO,
  // One for `ADVANCED_MEMORY_SAFETY_CHECKS()` objects.
  kAdvancedMemorySafetyChecks,
};

// Returns quarantine configuration for `process_name` and `branch_type`.
BASE_EXPORT ::partition_alloc::internal::SchedulerLoopQuarantineConfig
GetSchedulerLoopQuarantineConfiguration(
    const std::string& process_type,
    SchedulerLoopQuarantineBranchType branch_type);

}  // namespace base::allocator

#endif  // BASE_ALLOCATOR_SCHEDULER_LOOP_QUARANTINE_CONFIG_H_
