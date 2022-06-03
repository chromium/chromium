// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_

#include "base/allocator/allocator_shim.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/base_export.h"

namespace base {
namespace internal {

class BASE_EXPORT PartitionAllocMalloc {
 public:
  static ThreadSafePartitionRoot* Allocator();
  // May return |nullptr|, will never return the same pointer as |Allocator()|.
  static ThreadSafePartitionRoot* OriginalAllocator();
  // May return the same pointer as |Allocator()|.
  static ThreadSafePartitionRoot* AlignedAllocator();
};

BASE_EXPORT void* PartitionMalloc(const base::allocator::AllocatorDispatch*,
                                  size_t size,
                                  void* context);

BASE_EXPORT void* PartitionMallocUnchecked(
    const base::allocator::AllocatorDispatch*,
    size_t size,
    void* context);

BASE_EXPORT void* PartitionCalloc(const base::allocator::AllocatorDispatch*,
                                  size_t n,
                                  size_t size,
                                  void* context);

BASE_EXPORT void* PartitionMemalign(const base::allocator::AllocatorDispatch*,
                                    size_t alignment,
                                    size_t size,
                                    void* context);

BASE_EXPORT void* PartitionAlignedAlloc(
    const base::allocator::AllocatorDispatch* dispatch,
    size_t size,
    size_t alignment,
    void* context);

BASE_EXPORT void* PartitionAlignedRealloc(
    const base::allocator::AllocatorDispatch* dispatch,
    void* address,
    size_t size,
    size_t alignment,
    void* context);

BASE_EXPORT void* PartitionRealloc(const base::allocator::AllocatorDispatch*,
                                   void* address,
                                   size_t size,
                                   void* context);

BASE_EXPORT void PartitionFree(const base::allocator::AllocatorDispatch*,
                               void* address,
                               void* context);

BASE_EXPORT size_t
PartitionGetSizeEstimate(const base::allocator::AllocatorDispatch*,
                         void* address,
                         void* context);

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
