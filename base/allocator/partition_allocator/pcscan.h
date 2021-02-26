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

#if defined(__has_attribute)
#if __has_attribute(require_constant_initialization)
#define PA_CONSTINIT __attribute__((require_constant_initialization))
#else
#define PA_CONSTINIT
#endif
#endif

namespace base {
namespace internal {

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
// The driver class encapsulates the entire PCScan infrastructure. It provides
// a single function MoveToQuarantine() that posts a concurrent task if the
// limit is reached.
template <bool thread_safe>
class BASE_EXPORT PCScan final {
 public:
  using Root = PartitionRoot<thread_safe>;
  using SlotSpan = SlotSpanMetadata<thread_safe>;

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

  ALWAYS_INLINE void MoveToQuarantine(void* ptr, SlotSpan* slot_span);

  // Performs scanning only if a certain quarantine threshold was reached.
  void PerformScanIfNeeded(InvocationMode invocation_mode);

  void ClearRootsForTesting();

  bool IsInProgress() const {
    return in_progress_.load(std::memory_order_relaxed);
  }

 private:
  class PCScanTask;
  class PCScanThread;
  friend class PCScanTest;

  class QuarantineData final {
   public:
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

  static constexpr size_t kMaxNumberOfPartitions = 8u;

  // A custom constexpr container class that avoids dynamic initialization.
  // Constexprness is required to const-initialize the global PCScan.
  class Roots final : private std::array<Root*, kMaxNumberOfPartitions> {
    using Base = std::array<Root*, kMaxNumberOfPartitions>;

   public:
    using typename Base::const_iterator;
    using typename Base::iterator;

    // Explicitly value-initialize Base{} as otherwise the default
    // (aggregate) initialization won't be considered as constexpr.
    constexpr Roots() : Base{} {}

    iterator begin() { return Base::begin(); }
    const_iterator begin() const { return Base::begin(); }

    iterator end() { return begin() + current_; }
    const_iterator end() const { return begin() + current_; }

    void Add(Root* root);

    size_t size() const { return current_; }

    void ClearForTesting();

   private:
    size_t current_ = 0u;
  };

  constexpr PCScan() = default;

  // Performs scanning unconditionally.
  void PerformScan(InvocationMode invocation_mode);

  // Get size of all committed pages from scannable and nonscannable roots.
  size_t CalculateTotalHeapSize() const;

  static PCScan instance_ PA_CONSTINIT;

  Roots scannable_roots_{};
  Roots nonscannable_roots_{};
  QuarantineData quarantine_data_{};
  std::atomic<bool> in_progress_{false};
};

template <bool thread_safe>
bool PCScan<thread_safe>::QuarantineData::Account(size_t size) {
  size_t size_before = current_size_.fetch_add(size, std::memory_order_relaxed);
  return size_before + size > size_limit_.load(std::memory_order_relaxed);
}

template <bool thread_safe>
ALWAYS_INLINE void PCScan<thread_safe>::MoveToQuarantine(void* ptr,
                                                         SlotSpan* slot_span) {
  PA_DCHECK(!slot_span->bucket->is_direct_mapped());

  QuarantineBitmapFromPointer(QuarantineBitmapType::kMutator,
                              quarantine_data_.epoch(), ptr)
      ->SetBit(reinterpret_cast<uintptr_t>(ptr));

  const bool is_limit_reached =
      quarantine_data_.Account(slot_span->bucket->slot_size);
  if (UNLIKELY(is_limit_reached)) {
    // Perform a quick check if another scan is already in progress.
    if (in_progress_.load(std::memory_order_relaxed))
      return;
    // Avoid blocking the current thread for regular scans.
    PerformScan(InvocationMode::kNonBlocking);
  }
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PCSCAN_H_
