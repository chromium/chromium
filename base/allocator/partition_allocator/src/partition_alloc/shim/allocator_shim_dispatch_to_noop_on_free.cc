// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim_dispatch_to_noop_on_free.h"

#include <cstddef>

#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/shim/allocator_dispatch.h"
#include "partition_alloc/shim/allocator_shim.h"

namespace allocator_shim {
namespace {
void* AllocFn(const AllocatorDispatch* self, size_t size, void* context) {
  return self->next->alloc_function(self->next, size, context);
}

void* AllocUncheckedFn(const AllocatorDispatch* self,
                       size_t size,
                       void* context) {
  return self->next->alloc_unchecked_function(self->next, size, context);
}

void* AllocZeroInitializedFn(const AllocatorDispatch* self,
                             size_t n,
                             size_t size,
                             void* context) {
  return self->next->alloc_zero_initialized_function(self->next, n, size,
                                                     context);
}

void* AllocAlignedFn(const AllocatorDispatch* self,
                     size_t alignment,
                     size_t size,
                     void* context) {
  return self->next->alloc_aligned_function(self->next, alignment, size,
                                            context);
}

void* ReallocFn(const AllocatorDispatch* self,
                void* address,
                size_t size,
                void* context) {
  return self->next->realloc_function(self->next, address, size, context);
}

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {}

size_t GetSizeEstimateFn(const AllocatorDispatch* self,
                         void* address,
                         void* context) {
  return self->next->get_size_estimate_function(self->next, address, context);
}

size_t GoodSizeFn(const AllocatorDispatch* self, size_t size, void* context) {
  return self->next->good_size_function(self->next, size, context);
}

bool ClaimedAddressFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {
  return self->next->claimed_address_function(self->next, address, context);
}

unsigned BatchMallocFn(const AllocatorDispatch* self,
                       size_t size,
                       void** results,
                       unsigned num_requested,
                       void* context) {
  return self->next->batch_malloc_function(self->next, size, results,
                                           num_requested, context);
}

void BatchFreeFn(const AllocatorDispatch* self,
                 void** to_be_freed,
                 unsigned num_to_be_freed,
                 void* context) {}

void FreeDefiniteSizeFn(const AllocatorDispatch* self,
                        void* address,
                        size_t size,
                        void* context) {}

void TryFreeDefaultFn(const AllocatorDispatch* self,
                      void* address,
                      void* context) {}

static void* AlignedMallocFn(const AllocatorDispatch* self,
                             size_t size,
                             size_t alignment,
                             void* context) {
  return self->next->aligned_malloc_function(self->next, size, alignment,
                                             context);
}

static void* AlignedReallocFn(const AllocatorDispatch* self,
                              void* address,
                              size_t size,
                              size_t alignment,
                              void* context) {
  return self->next->aligned_realloc_function(self->next, address, size,
                                              alignment, context);
}

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {}

AllocatorDispatch allocator_dispatch = {
    &AllocFn,
    &AllocUncheckedFn,
    &AllocZeroInitializedFn,
    &AllocAlignedFn,
    &ReallocFn,
    &FreeFn,
    &GetSizeEstimateFn,
    &GoodSizeFn,
    &ClaimedAddressFn,
    &BatchMallocFn,
    &BatchFreeFn,
    &FreeDefiniteSizeFn,
    &TryFreeDefaultFn,
    &AlignedMallocFn,
    &AlignedReallocFn,
    &AlignedFreeFn,
    nullptr /* next */
};
}  // namespace

void InsertNoOpOnFreeAllocatorShimOnShutDown() {
  static bool called = false;
  PA_CHECK(!called);
  called = true;
  InsertAllocatorDispatch(&allocator_dispatch);
}

}  // namespace allocator_shim
