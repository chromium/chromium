// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_SAFETY_CHECKS_H_
#define BASE_MEMORY_SAFETY_CHECKS_H_

#include <cstdint>

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/memory/advanced_memory_safety_checks.h"
#include "base/memory/stack_allocated.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/scheduler_loop_quarantine_support.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base {

// Utility function to detect Double-Free or Out-of-Bounds writes.
// This function can be called to memory assumed to be valid.
// If not, this may crash (not guaranteed).
// This is useful if you want to investigate crashes at `free()`,
// to know which point at execution it goes wrong.
BASE_EXPORT void CheckHeapIntegrity(const void* ptr);

// The function here is called right before crashing with
// `DoubleFreeOrCorruptionDetected()`. We provide an address for the slot start
// to the function, and it may use that for debugging purpose.
void SetDoubleFreeOrCorruptionDetectedFn(void (*fn)(uintptr_t));

// Utility class to exclude deallocation from optional safety checks when an
// instance is on the stack. Can be applied to performance critical functions.
class BASE_EXPORT ScopedSafetyChecksExclusion {
  STACK_ALLOCATED();

 public:
  // Make this non-trivially-destructible to suppress unused variable warning.
  ~ScopedSafetyChecksExclusion() {}  // NOLINT(modernize-use-equals-default)

 private:
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  base::allocator::ScopedSchedulerLoopQuarantineExclusion
      opt_out_scheduler_loop_quarantine_;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
};

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
using base::allocator::SchedulerLoopQuarantineScanPolicyUpdater;
using base::allocator::ScopedSchedulerLoopQuarantineDisallowScanlessPurge;
#else
class SchedulerLoopQuarantineScanPolicyUpdater {
 public:
  ALWAYS_INLINE void DisallowScanlessPurge() {}
  ALWAYS_INLINE void AllowScanlessPurge() {}
};
class ScopedSchedulerLoopQuarantineDisallowScanlessPurge {};
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace base

#endif  // BASE_MEMORY_SAFETY_CHECKS_H_
