// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_
#define PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_

#include <optional>
#include <variant>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/memory/stack_allocated.h"
#include "partition_alloc/scheduler_loop_quarantine.h"
#include "partition_alloc/thread_cache.h"

// Extra utilities for Scheduler-Loop Quarantine.
// This is a separate header to avoid cyclic reference between "thread_cache.h"
// and "scheduler_loop_quarantine.h".

namespace partition_alloc {

struct PartitionRoot;

// When this class is alive, Scheduler-Loop Quarantine for this thread is
// paused and freed allocations will be freed immediately.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    ScopedSchedulerLoopQuarantineExclusion {
 public:
  ScopedSchedulerLoopQuarantineExclusion();
  ~ScopedSchedulerLoopQuarantineExclusion();

 private:
  std::optional<internal::ThreadBoundSchedulerLoopQuarantineBranch::
                    ScopedQuarantineExclusion>
      instance_;
};

// An utility class to update Scheduler-Loop Quarantine's purging strategy for
// the current thread. By default it uses "scanless" purge for best performance.
// However, it also supports stack-scanning before purging to verify there is no
// dangling pointer in stack memory. Stack-scanning comes with some performance
// cost, but there is security benefit. This class can be used to switch between
// these two strategies dynamically.
// An example usage is to allow scanless purge only around "stack bottom".
// We can safely assume there is no dangling pointer if stack memory is barely
// used thus safe to purge quarantine.
// At Chrome layer it is task execution and we expect
// `DisallowScanlessPurge()` to be called before task execution and
// `AllowScanlessPurge()` after. Since there is no unified way to hook
// task execution in Chrome, we provide an abstract utility here.
// This class is not thread-safe.
//
// TODO(http://crbug.com/329027914): stack-scanning is not implemented yet
// and this class is effectively "disallow any purge unless really needed".
// It still gives some hints on purging timing for memory efficiency.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    SchedulerLoopQuarantineScanPolicyUpdater {
 public:
  SchedulerLoopQuarantineScanPolicyUpdater();
  ~SchedulerLoopQuarantineScanPolicyUpdater();

  // Disallows scanless purge and performs stack-scanning when needed.
  // Can be called multiple times, but each call to this function must be
  // followed by `AllowScanlessPurge()`.
  void DisallowScanlessPurge();

  // Re-activate scanless purge. `DisallowScanlessPurge()` must be called prior
  // to use of this function. This may trigger purge immediately.
  void AllowScanlessPurge();

 private:
  PA_ALWAYS_INLINE internal::ThreadBoundSchedulerLoopQuarantineBranch*
  GetQuarantineBranch();

  uint32_t disallow_scanless_purge_calls_ = 0;

  // An address of `ThreadCache` instance works as a thread ID.
  uintptr_t tcache_address_ = 0;
};

// This is a lightweight version of `SchedulerLoopQuarantineScanPolicyUpdater`.
// It calls `DisallowScanlessPurge` in the constructor and `AllowScanlessPurge`
// in the destructor.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    ScopedSchedulerLoopQuarantineDisallowScanlessPurge {
  // This is `PA_STACK_ALLOCATED()` to ensure that those two calls are made on
  // the same thread, allowing us to omit thread-safety analysis.
  PA_STACK_ALLOCATED();

 public:
  PA_ALWAYS_INLINE ScopedSchedulerLoopQuarantineDisallowScanlessPurge() {
    ThreadCache* tcache = ThreadCache::EnsureAndGet();
    PA_CHECK(ThreadCache::IsValid(tcache));

    tcache->GetSchedulerLoopQuarantineBranch().DisallowScanlessPurge();
  }

  PA_ALWAYS_INLINE ~ScopedSchedulerLoopQuarantineDisallowScanlessPurge() {
    ThreadCache* tcache = ThreadCache::EnsureAndGet();
    PA_CHECK(ThreadCache::IsValid(tcache));

    tcache->GetSchedulerLoopQuarantineBranch().AllowScanlessPurge();
  }
};

namespace internal {
class PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    ScopedSchedulerLoopQuarantineBranchAccessorForTesting {
 public:
  explicit ScopedSchedulerLoopQuarantineBranchAccessorForTesting(
      PartitionRoot* allocator_root);
  ~ScopedSchedulerLoopQuarantineBranchAccessorForTesting();

  bool IsQuarantined(void* object);
  size_t GetCapacityInBytes();
  void Purge();

 private:
  std::variant<internal::GlobalSchedulerLoopQuarantineBranch*,
               internal::ThreadBoundSchedulerLoopQuarantineBranch*>
      branch_;
};

}  // namespace internal

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_SUPPORT_H_
