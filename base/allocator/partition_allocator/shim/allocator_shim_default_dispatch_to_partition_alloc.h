// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"
#include "base/base_export.h"

namespace allocator_shim::internal {

void PartitionAllocSetCallNewHandlerOnMallocFailure(bool value);

class BASE_EXPORT PartitionAllocMalloc {
 public:
  static partition_alloc::ThreadSafePartitionRoot* Allocator();
  // May return |nullptr|, will never return the same pointer as |Allocator()|.
  static partition_alloc::ThreadSafePartitionRoot* OriginalAllocator();
  // May return the same pointer as |Allocator()|.
  static partition_alloc::ThreadSafePartitionRoot* AlignedAllocator();
};

BASE_EXPORT void* PartitionMalloc(const AllocatorDispatch*,
                                  size_t size,
                                  void* context);

BASE_EXPORT void* PartitionMallocUnchecked(const AllocatorDispatch*,
                                           size_t size,
                                           void* context);

BASE_EXPORT void* PartitionCalloc(const AllocatorDispatch*,
                                  size_t n,
                                  size_t size,
                                  void* context);

BASE_EXPORT void* PartitionMemalign(const AllocatorDispatch*,
                                    size_t alignment,
                                    size_t size,
                                    void* context);

BASE_EXPORT void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                                        size_t size,
                                        size_t alignment,
                                        void* context);

BASE_EXPORT void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                                          void* address,
                                          size_t size,
                                          size_t alignment,
                                          void* context);

BASE_EXPORT void* PartitionRealloc(const AllocatorDispatch*,
                                   void* address,
                                   size_t size,
                                   void* context);

BASE_EXPORT void PartitionFree(const AllocatorDispatch*,
                               void* object,
                               void* context);

BASE_EXPORT size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                            void* address,
                                            void* context);

}  // namespace allocator_shim::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
