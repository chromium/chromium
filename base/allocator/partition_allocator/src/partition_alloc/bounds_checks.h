// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_BOUNDS_CHECKS_H_
#define PARTITION_ALLOC_BOUNDS_CHECKS_H_

// Herein are utilities that let callers determine whether two pointers
// belong to the same (PartitionAlloc) allocation.

#include <cstddef>
#include <cstdint>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc {

// Return values to indicate where a pointer is pointing relative to the bounds
// of an allocation.
enum class PtrPosWithinAlloc {
  // When BACKUP_REF_PTR_POISON_OOB_PTR is disabled, end-of-allocation pointers
  // are also considered in-bounds.
  kInBounds,
#if PA_BUILDFLAG(BACKUP_REF_PTR_POISON_OOB_PTR)
  kAllocEnd,
#endif
  kFarOOB
};

// Checks whether `test_address` is in the same allocation slot as
// `orig_address`.
//
// This can be called after adding or subtracting from the `orig_address`
// to produce a different pointer which must still stay in the same allocation.
//
// The `type_size` is the size of the type that the raw_ptr is pointing to,
// which may be the type the allocation is holding or a compatible pointer type
// such as a base class or char*. It is used to detect pointers near the end of
// the allocation but not strictly beyond it.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PtrPosWithinAlloc IsPtrWithinSameAllocInBRPPool(uintptr_t orig_address,
                                                uintptr_t test_address,
                                                size_t type_size);

// Similar to the above, but pool-agnostic and with different semantics.
// Used to support Checked Span (https://crbug.com/484171909).
//
// Note that this simply returns `false` for memory not managed by
// PartitionAlloc.
#if PA_BUILDFLAG(CHECKED_SPAN)
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsExtentOutOfBounds(const void* ptr,
                         size_t extent_bytes,
                         size_t type_size);
#else
PA_ALWAYS_INLINE constexpr bool IsExtentOutOfBounds(const void* ptr,
                                                    size_t extent_bytes,
                                                    size_t type_size) {
  return false;
}
#endif  // PA_BUILDFLAG(CHECKED_SPAN)

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_BOUNDS_CHECKS_H_
