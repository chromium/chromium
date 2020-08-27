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
#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/thread_annotations.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct PartitionBucket {
  // Accessed most in hot path => goes first.
  PartitionPage<thread_safe>* active_pages_head;

  PartitionPage<thread_safe>* empty_pages_head;
  PartitionPage<thread_safe>* decommitted_pages_head;
  uint32_t slot_size;
  uint32_t num_system_pages_per_slot_span : 8;
  uint32_t num_full_pages : 24;

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
  // Note the matching Free() functions are in PartitionPage.
  BASE_EXPORT NOINLINE void* SlowPathAlloc(PartitionRoot<thread_safe>* root,
                                           int flags,
                                           size_t size,
                                           bool* is_already_zeroed)
      EXCLUSIVE_LOCKS_REQUIRED(root->lock_);

  ALWAYS_INLINE bool is_direct_mapped() const {
    return !num_system_pages_per_slot_span;
  }
  ALWAYS_INLINE size_t get_bytes_per_span() const {
    // TODO(ajwong): Change to CheckedMul. https://crbug.com/787153
    // https://crbug.com/680657
    return num_system_pages_per_slot_span * kSystemPageSize;
  }
  ALWAYS_INLINE uint16_t get_slots_per_span() const {
    // TODO(ajwong): Change to CheckedMul. https://crbug.com/787153
    // https://crbug.com/680657
    return static_cast<uint16_t>(get_bytes_per_span() / slot_size);
  }

  static ALWAYS_INLINE size_t get_direct_map_size(size_t size) {
    // Caller must check that the size is not above the kMaxDirectMapped
    // limit before calling. This also guards against integer overflow in the
    // calculation here.
    PA_DCHECK(size <= kMaxDirectMapped);
    return (size + kSystemPageOffsetMask) & kSystemPageBaseMask;
  }

  BASE_EXPORT static PartitionBucket* get_sentinel_bucket();

  // This helper function scans a bucket's active page list for a suitable new
  // active page.  When it finds a suitable new active page (one that has
  // free slots and is not empty), it is set as the new active page. If there
  // is no suitable new active page, the current active page is set to
  // PartitionPage::get_sentinel_page(). As potential pages are scanned, they
  // are tidied up according to their state. Empty pages are swept on to the
  // empty page list, decommitted pages on to the decommitted page list and full
  // pages are unlinked from any list.
  //
  // This is where the guts of the bucket maintenance is done!
  bool SetNewActivePage();

  // Returns an offset within an allocation slot.
  ALWAYS_INLINE size_t GetSlotOffset(size_t offset_in_slot_span) {
    // Knowing that slots are tightly packed in a slot span, calculate an offset
    // using an equivalent of a modulo operation.

    // See the static assertion for `kReciprocalShift` above.
    PA_DCHECK(offset_in_slot_span <= kMaxBucketed);
    PA_DCHECK(slot_size <= kMaxBucketed);

    // Calculate `decimal_part{offset_in_slot / size} * (2 ** M)` first.
    uint64_t offset_in_slot =
        (offset_in_slot_span * slot_size_reciprocal) & kReciprocalMask;

    // (decimal_part * size) * (2 ** M) == offset_in_slot_span % size * (2 ** M)
    // Divide by `2 ** M` using a bit shift.
    offset_in_slot = (offset_in_slot * slot_size) >> kReciprocalShift;
    PA_DCHECK(offset_in_slot_span % slot_size == offset_in_slot);

    return static_cast<size_t>(offset_in_slot);
  }

 private:
  static NOINLINE void OnFull();

  // Returns a natural number of PartitionPages (calculated by
  // get_system_pages_per_slot_span()) to allocate from the current
  // SuperPage when the bucket runs out of slots.
  ALWAYS_INLINE uint16_t get_pages_per_slot_span();

  // Returns the number of system pages in a slot span.
  //
  // The calculation attemps to find the best number of System Pages to
  // allocate for the given slot_size to minimize wasted space. It uses a
  // heuristic that looks at number of bytes wasted after the last slot and
  // attempts to account for the PTE usage of each System Page.
  uint8_t get_system_pages_per_slot_span();

  // Allocates a new slot span with size |num_partition_pages| from the
  // current extent. Metadata within this slot span will be uninitialized.
  // Returns nullptr on error.
  ALWAYS_INLINE void* AllocNewSlotSpan(PartitionRoot<thread_safe>* root,
                                       int flags,
                                       uint16_t num_partition_pages)
      EXCLUSIVE_LOCKS_REQUIRED(root->lock_);

  // Each bucket allocates a slot span when it runs out of slots.
  // A slot span's size is equal to get_pages_per_slot_span() number of
  // PartitionPages. This function initializes all PartitionPage within the
  // span to point to the first PartitionPage which holds all the metadata
  // for the span and registers this bucket as the owner of the span. It does
  // NOT put the slots into the bucket's freelist.
  ALWAYS_INLINE void InitializeSlotSpan(PartitionPage<thread_safe>* page);

  // Allocates one slot from the given |page| and then adds the remainder to
  // the current bucket. If the |page| was freshly allocated, it must have been
  // passed through InitializeSlotSpan() first.
  ALWAYS_INLINE char* AllocAndFillFreelist(PartitionPage<thread_safe>* page);

  static PartitionBucket sentinel_bucket_;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_BUCKET_H_
