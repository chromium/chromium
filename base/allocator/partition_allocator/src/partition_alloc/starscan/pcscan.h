// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_STARSCAN_PCSCAN_H_
#define PARTITION_ALLOC_STARSCAN_PCSCAN_H_

#include <atomic>

#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/starscan/pcscan_scheduling.h"
#include "partition_alloc/tagging.h"

namespace partition_alloc {

class StatsReporter;

namespace internal {

[[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED
    PA_COMPONENT_EXPORT(PARTITION_ALLOC) void DoubleFreeAttempt();

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
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PCScan final {
 public:
  using Root = PartitionRoot;
  using SlotSpan = SlotSpanMetadata;

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

  // Parameters used to initialize *Scan.
  struct InitConfig {
    // Based on the provided mode, PCScan will try to use a certain
    // WriteProtector, if supported by the system.
    enum class WantedWriteProtectionMode : uint8_t {
      kDisabled,
      kEnabled,
    } write_protection = WantedWriteProtectionMode::kDisabled;

    // Flag that enables safepoints that stop mutator execution and help
    // scanning.
    enum class SafepointMode : uint8_t {
      kDisabled,
      kEnabled,
    } safepoint = SafepointMode::kDisabled;
  };

  PCScan(const PCScan&) = delete;
  PCScan& operator=(const PCScan&) = delete;

  // Initializes PCScan and prepares internal data structures.
  static void Initialize(InitConfig);
  static bool IsInitialized();

  // Disable/reenable PCScan. Temporal disabling can be useful in CPU demanding
  // contexts.
  static void Disable();
  static void Reenable();
  // Query if PCScan is enabled.
  static bool IsEnabled();

  // Registers a root for scanning.
  static void RegisterScannableRoot(Root* root);
  // Registers a root that doesn't need to be scanned but still contains
  // quarantined objects.
  static void RegisterNonScannableRoot(Root* root);

  // Registers a newly allocated super page for |root|.
  static void RegisterNewSuperPage(Root* root, uintptr_t super_page_base);

  PA_ALWAYS_INLINE static void MoveToQuarantine(void* object,
                                                size_t usable_size,
                                                uintptr_t slot_start,
                                                size_t slot_size);

  // Performs scanning unconditionally.
  static void PerformScan(InvocationMode invocation_mode);
  // Performs scanning only if a certain quarantine threshold was reached.
  static void PerformScanIfNeeded(InvocationMode invocation_mode);
  // Performs scanning with specified delay.
  static void PerformDelayedScan(int64_t delay_in_microseconds);

  // Enables safepoints in mutator threads.
  PA_ALWAYS_INLINE static void EnableSafepoints();
  // Join scan from safepoint in mutator thread. As soon as PCScan is scheduled,
  // mutators can join PCScan helping out with clearing and scanning.
  PA_ALWAYS_INLINE static void JoinScanIfNeeded();

  // Checks if there is a PCScan task currently in progress.
  PA_ALWAYS_INLINE static bool IsInProgress();

  // Sets process name (used for histograms). |name| must be a string literal.
  static void SetProcessName(const char* name);

  static void EnableStackScanning();
  static void DisableStackScanning();
  static bool IsStackScanningEnabled();

  static void EnableImmediateFreeing();

  // Define when clearing should happen (on free() or in scanning task).
  static void SetClearType(ClearType);

  static void UninitForTesting();

  static inline PCScanScheduler& scheduler();

  // Registers reporting class.
  static void RegisterStatsReporter(partition_alloc::StatsReporter* reporter);

 private:
  class PCScanThread;
  friend class PCScanTask;
  friend class PartitionAllocPCScanTestBase;
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

  PA_ALWAYS_INLINE static PCScan& Instance();

  PA_ALWAYS_INLINE bool IsJoinable() const;
  PA_ALWAYS_INLINE void SetJoinableIfSafepointEnabled(bool);

  inline constexpr PCScan();

  // Joins scan unconditionally.
  static void JoinScan();

  // Finish scan as scanner thread.
  static void FinishScanForTesting();

  // Reinitialize internal structures (e.g. card table).
  static void ReinitForTesting(InitConfig);

  size_t epoch() const { return scheduler_.epoch(); }

  // PA_CONSTINIT for fast access (avoiding static thread-safe initialization).
  static PCScan instance_ PA_CONSTINIT;

  PCScanScheduler scheduler_{};
  std::atomic<State> state_{State::kNotRunning};
  std::atomic<bool> is_joinable_{false};
  bool is_safepoint_enabled_{false};
  ClearType clear_type_{ClearType::kLazy};
};

// To please Chromium's clang plugin.
constexpr PCScan::PCScan() = default;

PA_ALWAYS_INLINE PCScan& PCScan::Instance() {
  // The instance is declared as a static member, not static local. The reason
  // is that we want to use the require_constant_initialization attribute to
  // avoid double-checked-locking which would otherwise have been introduced
  // by the compiler for thread-safe dynamic initialization (see constinit
  // from C++20).
  return instance_;
}

PA_ALWAYS_INLINE bool PCScan::IsInProgress() {
  const PCScan& instance = Instance();
  return instance.state_.load(std::memory_order_relaxed) != State::kNotRunning;
}

PA_ALWAYS_INLINE bool PCScan::IsJoinable() const {
  // This has acquire semantics since a mutator relies on the task being set up.
  return is_joinable_.load(std::memory_order_acquire);
}

PA_ALWAYS_INLINE void PCScan::SetJoinableIfSafepointEnabled(bool value) {
  if (!is_safepoint_enabled_) {
    PA_DCHECK(!is_joinable_.load(std::memory_order_relaxed));
    return;
  }
  // Release semantics is required to "publish" the change of the state so that
  // the mutators can join scanning and expect the consistent state.
  is_joinable_.store(value, std::memory_order_release);
}

PA_ALWAYS_INLINE void PCScan::EnableSafepoints() {
  PCScan& instance = Instance();
  instance.is_safepoint_enabled_ = true;
}

PA_ALWAYS_INLINE void PCScan::JoinScanIfNeeded() {
  PCScan& instance = Instance();
  if (PA_UNLIKELY(instance.IsJoinable())) {
    instance.JoinScan();
  }
}

PA_ALWAYS_INLINE void PCScan::MoveToQuarantine(void* object,
                                               size_t usable_size,
                                               uintptr_t slot_start,
                                               size_t slot_size) {
  PCScan& instance = Instance();
  if (instance.clear_type_ == ClearType::kEager) {
    // We need to distinguish between usable_size and slot_size in this context:
    // - for large buckets usable_size can be noticeably smaller than slot_size;
    // - usable_size is safe as it doesn't cover extras as opposed to slot_size.
    // TODO(bikineev): If we start protecting quarantine memory, we can lose
    // double-free coverage (the check below). Consider performing the
    // double-free check before protecting if eager clearing becomes default.
    SecureMemset(object, 0, usable_size);
  }

  auto* state_bitmap = StateBitmapFromAddr(slot_start);

  // Mark the state in the state bitmap as quarantined. Make sure to do it after
  // the clearing to avoid racing with *Scan Sweeper.
  [[maybe_unused]] const bool succeeded =
      state_bitmap->Quarantine(slot_start, instance.epoch());
#if PA_CONFIG(STARSCAN_EAGER_DOUBLE_FREE_DETECTION_ENABLED)
  if (PA_UNLIKELY(!succeeded)) {
    DoubleFreeAttempt();
  }
#else
  // The compiler is able to optimize cmpxchg to a lock-prefixed and.
#endif  // PA_CONFIG(STARSCAN_EAGER_DOUBLE_FREE_DETECTION_ENABLED)

  const bool is_limit_reached = instance.scheduler_.AccountFreed(slot_size);
  if (PA_UNLIKELY(is_limit_reached)) {
    // Perform a quick check if another scan is already in progress.
    if (instance.IsInProgress()) {
      return;
    }
    // Avoid blocking the current thread for regular scans.
    instance.PerformScan(InvocationMode::kNonBlocking);
  }
}

inline PCScanScheduler& PCScan::scheduler() {
  PCScan& instance = Instance();
  return instance.scheduler_;
}

}  // namespace internal
}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_STARSCAN_PCSCAN_H_
