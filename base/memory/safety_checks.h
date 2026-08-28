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
#include "base/pending_task.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/safety_checks.h"  // nogncheck

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/scheduler_loop_quarantine_support.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace base {

// The function here is called right before crashing with
// `DoubleFreeOrCorruptionDetected()`. We provide an address for the slot start
// to the function, and it may use that for debugging purpose.
void SetDoubleFreeOrCorruptionDetectedFn(void (*fn)(uintptr_t));

using partition_alloc::ScopedSafetyChecksExclusion;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
using base::allocator::QuarantineTaskType;
using base::allocator::SchedulerLoopQuarantineScanPolicyUpdater;
struct ScopedSchedulerLoopQuarantineTaskScope {
  STACK_ALLOCATED();

 public:
  ALWAYS_INLINE explicit ScopedSchedulerLoopQuarantineTaskScope(
      const PendingTask& pending_task)
      : task_scope_(pending_task.ipc_interface_name != nullptr ||
                            pending_task.ipc_hash != 0
                        ? QuarantineTaskType::kMojoIPC
                        : QuarantineTaskType::kNormal) {}

  base::allocator::ScopedSchedulerLoopQuarantineTaskScope task_scope_;
};
#else
enum class QuarantineTaskType {
  kNormal,
  kMojoIPC,
};
class SchedulerLoopQuarantineScanPolicyUpdater {
 public:
  ALWAYS_INLINE void DisallowScanlessPurge() {}
  ALWAYS_INLINE void AllowScanlessPurge() {}
};
struct ScopedSchedulerLoopQuarantineTaskScope {
  STACK_ALLOCATED();

 public:
  constexpr explicit ScopedSchedulerLoopQuarantineTaskScope(
      const PendingTask& pending_task) {}
};
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace base

#endif  // BASE_MEMORY_SAFETY_CHECKS_H_
