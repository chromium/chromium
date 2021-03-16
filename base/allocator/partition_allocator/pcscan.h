// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PCSCAN_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PCSCAN_H_

#include <atomic>

#include "base/allocator/partition_allocator/object_bitmap.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"

#if defined(__has_attribute)
#if __has_attribute(require_constant_initialization)
#define PA_CONSTINIT __attribute__((require_constant_initialization))
#else
#define PA_CONSTINIT
#endif
#endif

namespace base {
namespace internal {

class PCScanTask;

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
  };

  static PCScan& Instance() {
    // The instance is declared as a static member, not static local. The reason
    // is that we want to use the require_constant_initialization attribute to
    // avoid double-checked-locking which would otherwise have been introduced
    // by the compiler for thread-safe dynamic initialization (see constinit
    // from C++20).
    return instance_;
  }

  PCScan(const PCScan&) = delete;
  PCScan& operator=(const PCScan&) = delete;

  // Registers a root for scanning.
  void RegisterScannableRoot(Root* root);
  // Registers a root that doesn't need to be scanned but still contains
  // quarantined objects.
  void RegisterNonScannableRoot(Root* root);

  ALWAYS_INLINE void MoveToQuarantine(void* ptr, size_t slot_size);

  // Performs scanning only if a certain quarantine threshold was reached.
  void PerformScanIfNeeded(InvocationMode invocation_mode);

  // Checks if there is a PCScan task currently in progress.
  ALWAYS_INLINE bool IsInProgress() const;

  // Sets process name (used for histograms). |name| must be a string literal.
  void SetProcessName(const char* name);

  void ClearRootsForTesting();

 private:
  class PCScanThread;
  friend class PCScanTask;
  friend class PCScanTest;

  class QuarantineData final {
   public:
    inline constexpr QuarantineData();

    // Account freed bytes. Returns true if limit was reached.
    ALWAYS_INLINE bool Account(size_t bytes);

    void GrowLimitIfNeeded(size_t heap_size);
    void ResetAndAdvanceEpoch();

    size_t epoch() const { return epoch_.load(std::memory_order_relaxed); }
    size_t size() const {
      return current_size_.load(std::memory_order_relaxed);
    }
    size_t last_size() const { return last_size_; }

    bool MinimumScanningThresholdReached() const {
      return size() > kQuarantineSizeMinLimit;
    }

   private:
    static constexpr size_t kQuarantineSizeMinLimit = 1 * 1024 * 1024;

    std::atomic<size_t> current_size_{0u};
    std::atomic<size_t> size_limit_{kQuarantineSizeMinLimit};
    std::atomic<size_t> epoch_{0u};
    size_t last_size_ = 0;
  };

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

  inline constexpr PCScan();

  // Performs scanning unconditionally.
  void PerformScan(InvocationMode invocation_mode);

  // PA_CONSTINIT for fast access (avoiding static thread-safe initialization).
  static PCScan instance_ PA_CONSTINIT;

  QuarantineData quarantine_data_{};
  std::atomic<State> state_{State::kNotRunning};
};

// To please Chromium's clang plugin.
constexpr PCScan::QuarantineData::QuarantineData() = default;

ALWAYS_INLINE bool PCScan::QuarantineData::Account(size_t size) {
  size_t size_before = current_size_.fetch_add(size, std::memory_order_relaxed);
  return size_before + size > size_limit_.load(std::memory_order_relaxed);
}

// To please Chromium's clang plugin.
constexpr PCScan::PCScan() = default;

ALWAYS_INLINE bool PCScan::IsInProgress() const {
  return state_.load(std::memory_order_relaxed) != State::kNotRunning;
}

ALWAYS_INLINE void PCScan::MoveToQuarantine(void* ptr, size_t slot_size) {
  auto* quarantine = QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                                                 quarantine_data_.epoch(), ptr);
  const bool is_double_freed =
      quarantine->SetBit(reinterpret_cast<uintptr_t>(ptr));
  if (UNLIKELY(is_double_freed))
    DoubleFreeAttempt();

  const bool is_limit_reached = quarantine_data_.Account(slot_size);
  if (UNLIKELY(is_limit_reached)) {
    // Perform a quick check if another scan is already in progress.
    if (IsInProgress())
      return;
    // Avoid blocking the current thread for regular scans.
    PerformScan(InvocationMode::kNonBlocking);
  }
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PCSCAN_H_
