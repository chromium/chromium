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
//
// TODO: PCScan should work for all partitions in the PartitionAlloc heap, not
// on a per-PartitionRoot basis.
template <bool thread_safe>
class BASE_EXPORT PCScan final {
 public:
  using Root = PartitionRoot<thread_safe>;
  using SlotSpan = SlotSpanMetadata<thread_safe>;

  enum class InvocationMode {
    kBlocking,
    kNonBlocking,
  };

  explicit PCScan(Root* root);

  PCScan(const PCScan&) = delete;
  PCScan& operator=(const PCScan&) = delete;

  ~PCScan();

  ALWAYS_INLINE void MoveToQuarantine(void* ptr, SlotSpan* slot_span);

  void PerformScanIfNeeded(InvocationMode invocation_mode);

 private:
  class PCScanTask;
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

  void PerformScan(InvocationMode invocation_mode);

  Root* root_;
  QuarantineData quarantine_data_;
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
  if (is_limit_reached) {
    // Avoid blocking the current thread for regular scans.
    PerformScan(InvocationMode::kNonBlocking);
  }
}

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PCSCAN_H_
