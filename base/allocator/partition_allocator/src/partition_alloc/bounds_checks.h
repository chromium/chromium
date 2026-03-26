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
#include "partition_alloc/partition_alloc_base/numerics/checked_math.h"
#include "partition_alloc/partition_alloc_base/numerics/safe_conversions.h"

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

// Prefer to use the templated version below this function.
//
// Pool-agnostic version of `IsPtrWithinSameAllocInBRPPool()`. Primarily
// used to support Checked Span (https://crbug.com/484171909).
//
// Note:
//
// *  This function returns `false` for memory not managed by
//    PartitionAlloc.
//
// *  TODO(crbug.com/484171909): This function currently only supports
//    64-bit platforms. It always returns `false` on 32-bit.
//
// *  TODO(crbug.com/484171909): This function must be used after
//    PartitionAlloc (specifically, the AddressPoolManager) is
//    initialized. Data races will occur if this function is called too
//    early. (See the TSan trybots on https://crrev.com/c/7673121 for
//    examples.)
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool IsExtentOutOfBounds(const void* ptr,
                         size_t extent_bytes,
                         size_t type_size);

// Suitable for external callers. Has the same caveats as
// `IsExtentOutOfBounds()` (but inverted).
//
// Given a `T* elems` and a max `index`, call
// ```
// CHECK(IsExtentInBounds(elems, index));
// ```
template <typename T>
bool IsExtentInBounds(const T* ptr,
                      internal::base::StrictNumeric<size_t> index) {
  internal::base::CheckedNumeric<size_t> size_bytes = index;
  size_bytes *= sizeof(T);
  return !IsExtentOutOfBounds(ptr, size_bytes.ValueOrDie(), sizeof(T));
}

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_BOUNDS_CHECKS_H_
