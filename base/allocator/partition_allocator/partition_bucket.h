// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_H_

#include <stddef.h>
#include <stdint.h>

#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/thread_annotations.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct PartitionBucket {
  // Accessed most in hot path => goes first. Only nullptr for invalid buckets,
  // may be pointing to the sentinel.
  SlotSpanMetadata<thread_safe>* active_slot_spans_head;

  SlotSpanMetadata<thread_safe>* empty_slot_spans_head;
  SlotSpanMetadata<thread_safe>* decommitted_slot_spans_head;
  uint32_t slot_size;
  uint32_t num_system_pages_per_slot_span : 8;
  uint32_t num_full_slot_spans : 24;

  // `slot_size_reciprocal` is used to improve the performance of
  // `GetSlotOffset`. It is computed as `(1 / size) * (2 ** M)` where M is
  // chosen to provide the desired accuracy. As a result, we can replace a slow
  // integer division (or modulo) operation with a pair of multiplication and a
  // bit shift, i.e. `value / size` becomes `(value * size_reciprocal) >> M`.
  uint64_t slot_size_reciprocal;

  // This is `M` from the formula above. For accurate results, both `value` and
  // `size`, which are bound by `kMaxBucketed` for our purposes, must be less
  // than `2 ** (M / 2)`. On the other hand, the result of the expression
  // `3 * M / 2` must be less than 64, otherwise integer overflow can occur.
  static constexpr uint64_t kReciprocalShift = 42;
  static constexpr uint64_t kReciprocalMask = (1ull << kReciprocalShift) - 1;
  static_assert(
      kMaxBucketed < (1 << (kReciprocalShift / 2)),
      "GetSlotOffset may produce an incorrect result when kMaxBucketed is too "
      "large.");

  // Public API.
  void Init(uint32_t new_slot_size);

  // Sets |is_already_zeroed| to true if the allocation was satisfied by
  // requesting (a) new page(s) from the operating system, or false otherwise.
  // This enables an optimization for when callers use |PartitionAllocZeroFill|:
  // there is no need to call memset on fresh pages; the OS has already zeroed
  // them. (See |PartitionRoot::AllocFromBucket|.)
  //
  // Note the matching Free() functions are in SlotSpanMetadata.
  BASE_EXPORT NOINLINE void* SlowPathAlloc(PartitionRoot<thread_safe>* root,
                                           int flags,
                                           size_t raw_size,
                                           size_t slot_span_alignment,
                                           bool* is_already_zeroed)
      EXCLUSIVE_LOCKS_REQUIRED(root->lock_);

  ALWAYS_INLINE bool CanStoreRawSize() const {
    // For direct-map as well as single-slot slot spans (recognized by checking
    // against |MaxRegularSlotSpanSize()|), we have some spare metadata space in
    // subsequent PartitionPage to store the raw size. It isn't only metadata
    // space though, slot spans that have more than one slot can't have raw size
    // stored, because we wouldn't know which slot it applies to.
    if (LIKELY(slot_size <= MaxRegularSlotSpanSize()))
      return false;

    PA_DCHECK((slot_size % SystemPageSize()) == 0);
    PA_DCHECK(is_direct_mapped() || get_slots_per_span() == 1);

    return true;
  }

  // Some buckets are pseudo-buckets, which are disabled because they would
  // otherwise not fulfill alignment constraints.
  ALWAYS_INLINE bool is_valid() const {
    return active_slot_spans_head != nullptr;
  }
  ALWAYS_INLINE bool is_direct_mapped() const {
    return !num_system_pages_per_slot_span;
  }
  ALWAYS_INLINE size_t get_bytes_per_span() const {
    // TODO(ajwong): Change to CheckedMul. https://crbug.com/787153
    // https://crbug.com/680657
    return num_system_pages_per_slot_span << SystemPageShift();
  }
  ALWAYS_INLINE uint16_t get_slots_per_span() const {
    // TODO(ajwong): Change to CheckedMul. https://crbug.com/787153
    // https://crbug.com/680657
    return static_cast<uint16_t>(get_bytes_per_span() / slot_size);
  }
  // Returns a natural number of partition pages (calculated by
  // get_system_pages_per_slot_span()) to allocate from the current
  // super page when the bucket runs out of slots.
  ALWAYS_INLINE uint16_t get_pages_per_slot_span() const {
    // Rounds up to nearest multiple of NumSystemPagesPerPartitionPage().
    return (num_system_pages_per_slot_span +
            (NumSystemPagesPerPartitionPage() - 1)) /
           NumSystemPagesPerPartitionPage();
  }

  // This helper function scans a bucket's active slot span list for a suitable
  // new active slot span.  When it finds a suitable new active slot span (one
  // that has free slots and is not empty), it is set as the new active slot
  // span. If there is no suitable new active slot span, the current active slot
  // span is set to SlotSpanMetadata::get_sentinel_slot_span(). As potential
  // slot spans are scanned, they are tidied up according to their state. Empty
  // slot spans are swept on to the empty list, decommitted slot spans on to the
  // decommitted list and full slot spans are unlinked from any list.
  //
  // This is where the guts of the bucket maintenance is done!
  bool SetNewActiveSlotSpan();

  // Returns a slot number starting from the beginning of the slot span.
  ALWAYS_INLINE size_t GetSlotNumber(size_t offset_in_slot_span) {
    // See the static assertion for `kReciprocalShift` above.
    PA_DCHECK(offset_in_slot_span <= kMaxBucketed);
    PA_DCHECK(slot_size <= kMaxBucketed);

    const size_t offset_in_slot =
        ((offset_in_slot_span * slot_size_reciprocal) >> kReciprocalShift);
    PA_DCHECK(offset_in_slot_span / slot_size == offset_in_slot);

    return offset_in_slot;
  }

 private:
  static NOINLINE void OnFull();

  // Returns the number of system pages in a slot span.
  //
  // The calculation attempts to find the best number of system pages to
  // allocate for the given slot_size to minimize wasted space. It uses a
  // heuristic that looks at number of bytes wasted after the last slot and
  // attempts to account for the PTE usage of each system page.
  uint8_t get_system_pages_per_slot_span();

  // Allocates a new slot span with size |num_partition_pages| from the
  // current extent. Metadata within this slot span will be initialized.
  // Returns nullptr on error.
  ALWAYS_INLINE SlotSpanMetadata<thread_safe>* AllocNewSlotSpan(
      PartitionRoot<thread_safe>* root,
      int flags,
      size_t slot_span_alignment) EXCLUSIVE_LOCKS_REQUIRED(root->lock_);

  // Allocates a new super page from the current extent. All slot-spans will be
  // in the decommitted state. Returns nullptr on error.
  ALWAYS_INLINE void* AllocNewSuperPage(PartitionRoot<thread_safe>* root)
      EXCLUSIVE_LOCKS_REQUIRED(root->lock_);

  // Each bucket allocates a slot span when it runs out of slots.
  // A slot span's size is equal to get_pages_per_slot_span() number of
  // partition pages. This function initializes all PartitionPage within the
  // span to point to the first PartitionPage which holds all the metadata
  // for the span (in PartitionPage::SlotSpanMetadata) and registers this bucket
  // as the owner of the span. It does NOT put the slots into the bucket's
  // freelist.
  ALWAYS_INLINE void InitializeSlotSpan(
      SlotSpanMetadata<thread_safe>* slot_span);

  // Commit 1 or more pages in |slot_span|, enough to get the next slot, which
  // is returned by this function. If more slots fit into the committed pages,
  // they'll be added to the free list of the slot span (note that next pointers
  // are stored inside the slots).
  // The free list must be empty when calling this function.
  //
  // If |slot_span| was freshly allocated, it must have been passed through
  // InitializeSlotSpan() first.
  ALWAYS_INLINE char* ProvisionMoreSlotsAndAllocOne(
      PartitionRoot<thread_safe>* root,
      SlotSpanMetadata<thread_safe>* slot_span)
      EXCLUSIVE_LOCKS_REQUIRED(root->lock_);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_H_
