// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_H_

#include <atomic>

#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/starscan/pcscan_scheduling.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"

#define PCSCAN_DISABLE_SAFEPOINTS 0

namespace base {
namespace internal {

[[noreturn]] BASE_EXPORT NOINLINE NOT_TAIL_CALLED void DoubleFreeAttempt();

// PCScan (Probabilistic Conservative Scanning) is the algorithm that eliminates
// use-after-free bugs by verifying that there are no pointers in memory which
// point to explicitly freed objects before actually releasing their memory. If
// PCScan is enabled for a partition, freed objects are not immediately returned
// to the allocator, but are stored in a quarantine. When the quarantine reaches
// a certain threshold, a concurrent PCScan task gets posted. The task scans the
// entire heap, looking for dangling pointers (those that point to the
// quarantine entries). After scanning, the unvisited quarantine entries are
// unreachable and therefore can be safely reclaimed.
//
// The driver class encapsulates the entire PCScan infrastructure.
class BASE_EXPORT PCScan final {
 public:
  using Root = PartitionRoot<ThreadSafe>;
  using SlotSpan = SlotSpanMetadata<ThreadSafe>;

  enum class InvocationMode {
    kBlocking,
    kNonBlocking,
    kForcedBlocking,
    kScheduleOnlyForTesting,
  };

  enum class ClearType : uint8_t {
    // Clear in the scanning task.
    kLazy,
    // Eagerly clear quarantined objects on MoveToQuarantine().
    kEager,
  };

  // Based on the provided mode, PCScan will try to use a certain
  // WriteProtector, if supported by the system.
  enum class WantedWriteProtectionMode : uint8_t {
    kDisabled,
    kEnabled,
  };

  PCScan(const PCScan&) = delete;
  PCScan& operator=(const PCScan&) = delete;

  // Initializes PCScan and prepares internal data structures.
  static void Initialize(WantedWriteProtectionMode);

  // Registers a root for scanning.
  static void RegisterScannableRoot(Root* root);
  // Registers a root that doesn't need to be scanned but still contains
  // quarantined objects.
  static void RegisterNonScannableRoot(Root* root);

  // Registers a newly allocated super page for |root|.
  static void RegisterNewSuperPage(Root* root, uintptr_t super_page_base);
  static void RegisterNewSuperPage(PartitionRoot<NotThreadSafe>* root,
                                   uintptr_t super_page_base) {}

  ALWAYS_INLINE static void MoveToQuarantine(void* ptr,
                                             size_t usable_size,
                                             size_t slot_size);

  // Performs scanning only if a certain quarantine threshold was reached.
  static void PerformScanIfNeeded(InvocationMode invocation_mode);

  static void PerformDelayedScan(TimeDelta delay);

  // Join scan from safepoint in mutator thread. As soon as PCScan is scheduled,
  // mutators can join PCScan helping out with clearing and scanning.
  static void JoinScanIfNeeded();

  // Checks if there is a PCScan task currently in progress.
  ALWAYS_INLINE static bool IsInProgress();

  // Sets process name (used for histograms). |name| must be a string literal.
  static void SetProcessName(const char* name);

  static void EnableStackScanning();
  static void DisableStackScanning();
  static bool IsStackScanningEnabled();

  static void EnableImmediateFreeing();

  // Notify PCScan that a new thread was created/destroyed. Can be called for
  // uninitialized PCScan (before Initialize()).
  static void NotifyThreadCreated(void* stack_top);
  static void NotifyThreadDestroyed();

  // Define when clearing should happen (on free() or in scanning task).
  static void SetClearType(ClearType);

  static void UninitForTesting();

  inline static PCScanScheduler& scheduler();

 private:
  class PCScanThread;
  friend class PCScanTask;
  friend class PartitionAllocPCScanTest;
  friend class PCScanInternal;

  enum class State : uint8_t {
    // PCScan task is not scheduled.
    kNotRunning,
    // PCScan task is being started and about to be scheduled.
    kScheduled,
    // PCScan task is scheduled and can be scanning (or clearing).
    kScanning,
    // PCScan task is sweeping or finalizing.
    kSweepingAndFinishing
  };

  ALWAYS_INLINE static PCScan& Instance();

  ALWAYS_INLINE bool IsJoinable() const;

  inline constexpr PCScan();

  // Performs scanning unconditionally.
  void PerformScan(InvocationMode invocation_mode);

  // Joins scan unconditionally.
  static void JoinScan();

  // Finish scan as scanner thread.
  static void FinishScanForTesting();

  // Reinitialize internal structures (e.g. card table).
  static void ReinitForTesting(WantedWriteProtectionMode);

  size_t epoch() const { return scheduler_.epoch(); }

  // CONSTINIT for fast access (avoiding static thread-safe initialization).
  static PCScan instance_ CONSTINIT;

  PCScanScheduler scheduler_{};
  std::atomic<State> state_{State::kNotRunning};
  ClearType clear_type_{ClearType::kLazy};
};

// To please Chromium's clang plugin.
constexpr PCScan::PCScan() = default;

ALWAYS_INLINE PCScan& PCScan::Instance() {
  // The instance is declared as a static member, not static local. The reason
  // is that we want to use the require_constant_initialization attribute to
  // avoid double-checked-locking which would otherwise have been introduced
  // by the compiler for thread-safe dynamic initialization (see constinit
  // from C++20).
  return instance_;
}

ALWAYS_INLINE bool PCScan::IsInProgress() {
  const PCScan& instance = Instance();
  return instance.state_.load(std::memory_order_relaxed) != State::kNotRunning;
}

ALWAYS_INLINE bool PCScan::IsJoinable() const {
  // We can only join PCScan in the mutator if it's running and not sweeping.
  // This has acquire semantics since a mutator relies on the task being set up.
  return state_.load(std::memory_order_acquire) == State::kScanning;
}

ALWAYS_INLINE void PCScan::JoinScanIfNeeded() {
  PCScan& instance = Instance();
  if (UNLIKELY(instance.IsJoinable()))
    instance.JoinScan();
}

ALWAYS_INLINE void PCScan::MoveToQuarantine(void* ptr,
                                            size_t usable_size,
                                            size_t slot_size) {
  PCScan& instance = Instance();
  auto* quarantine = QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                                                 instance.epoch(), ptr);
  const bool is_double_freed =
      quarantine->SetBit(reinterpret_cast<uintptr_t>(ptr));
  if (UNLIKELY(is_double_freed))
    DoubleFreeAttempt();

  const bool is_limit_reached = instance.scheduler_.AccountFreed(slot_size);
  if (instance.clear_type_ == ClearType::kEager) {
    // We need to distinguish between usable_size and slot_size in this context:
    // - for large buckets usable_size can be noticeably smaller than slot_size;
    // - usable_size is safe as it doesn't cover extras as opposed to slot_size.
    memset(ptr, 0, usable_size);
  }

  if (UNLIKELY(is_limit_reached)) {
    // Perform a quick check if another scan is already in progress.
    if (instance.IsInProgress())
      return;
    // Avoid blocking the current thread for regular scans.
    instance.PerformScan(InvocationMode::kNonBlocking);
  }
}

inline PCScanScheduler& PCScan::scheduler() {
  PCScan& instance = Instance();
  return instance.scheduler_;
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_PCSCAN_H_
