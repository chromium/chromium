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

void FreeFn(const AllocatorDispatch* self, void* address, void* context) {}

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

static void AlignedFreeFn(const AllocatorDispatch* self,
                          void* address,
                          void* context) {}

AllocatorDispatch allocator_dispatch = {
    nullptr,             // alloc_function
    nullptr,             // alloc_unchecked_function
    nullptr,             // alloc_zero_initialized_function
    nullptr,             // alloc_aligned_function
    nullptr,             // realloc_function
    FreeFn,              // free_function
    nullptr,             // get_size_estimate_function
    nullptr,             // good_size_function
    nullptr,             // claimed_address_function
    nullptr,             // batch_malloc_function
    BatchFreeFn,         // batch_free_function
    FreeDefiniteSizeFn,  // free_definite_size_function
    TryFreeDefaultFn,    // try_free_default_function
    nullptr,             // aligned_malloc_function
    nullptr,             // aligned_realloc_function
    AlignedFreeFn,       // aligned_free_function
    nullptr              // next
};

}  // namespace

void InsertNoOpOnFreeAllocatorShimOnShutDown() {
  static bool called = false;
  PA_CHECK(!called);
  called = true;
  InsertAllocatorDispatch(&allocator_dispatch);
}

}  // namespace allocator_shim
