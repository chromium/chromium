// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/shim/allocator_shim.h"

namespace allocator_shim::internal {

void PartitionAllocSetCallNewHandlerOnMallocFailure(bool value);

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionAllocMalloc {
 public:
  // Returns true if ConfigurePartitions() has completed, meaning that the
  // allocators are effectively set in stone.
  static bool AllocatorConfigurationFinalized();

  static partition_alloc::PartitionRoot* Allocator();
  // May return |nullptr|, will never return the same pointer as |Allocator()|.
  static partition_alloc::PartitionRoot* OriginalAllocator();
  // May return the same pointer as |Allocator()|.
  static partition_alloc::PartitionRoot* AlignedAllocator();
};

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionMallocUnchecked(const AllocatorDispatch*,
                               size_t size,
                               void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionAlignedAlloc(const AllocatorDispatch* dispatch,
                            size_t size,
                            size_t alignment,
                            void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionAlignedRealloc(const AllocatorDispatch* dispatch,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionFree(const AllocatorDispatch*, void* object, void* context);

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context);

}  // namespace allocator_shim::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SHIM_ALLOCATOR_SHIM_DEFAULT_DISPATCH_TO_PARTITION_ALLOC_H_
