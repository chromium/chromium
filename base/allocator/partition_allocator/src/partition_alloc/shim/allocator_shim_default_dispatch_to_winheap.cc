// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <ostream>

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/shim/allocator_shim.h"
#include "partition_alloc/shim/winheap_stubs_win.h"

namespace {

using allocator_shim::AllocatorDispatch;

void* DefaultWinHeapMallocImpl(size_t size, AllocToken, void* context) {
  return allocator_shim::WinHeapMalloc(size);
}

void* DefaultWinHeapCallocImpl(size_t n,
                               size_t elem_size,
                               AllocToken alloc_token,
                               void* context) {
  // Overflow check.
  const size_t size = n * elem_size;
  if (elem_size != 0 && size / elem_size != n) {
    return nullptr;
  }

  void* result = DefaultWinHeapMallocImpl(size, alloc_token, context);
  if (result) {
    PA_UNSAFE_BUFFERS(memset(result, 0, size));
  }
  return result;
}

void* DefaultWinHeapMemalignImpl(size_t alignment,
                                 size_t size,
                                 AllocToken,
                                 void* context) {
  PA_CHECK(false) << "The windows heap does not support memalign.";
  return nullptr;
}

void* DefaultWinHeapReallocImpl(void* address,
                                size_t size,
                                AllocToken,
                                void* context) {
  return allocator_shim::WinHeapRealloc(address, size);
}

void DefaultWinHeapFreeImpl(void* address, void* context) {
  allocator_shim::WinHeapFree(address);
}

void DefaultWinHeapFreeWithSizeImpl(void* address, size_t size, void* context) {
  allocator_shim::WinHeapFree(address);
}

void DefaultWinHeapFreeWithAlignmentImpl(void* address,
                                         size_t alignment,
                                         void* context) {
  allocator_shim::WinHeapFree(address);
}

void DefaultWinHeapFreeWithSizeAndAlignmentImpl(void* address,
                                                size_t size,
                                                size_t alignment,
                                                void* context) {
  allocator_shim::WinHeapFree(address);
}

size_t DefaultWinHeapGetSizeEstimateImpl(void* address, void* context) {
  return allocator_shim::WinHeapGetSizeEstimate(address);
}

void* DefaultWinHeapAlignedMallocImpl(size_t size,
                                      size_t alignment,
                                      AllocToken,
                                      void* context) {
  return allocator_shim::WinHeapAlignedMalloc(size, alignment);
}

void* DefaultWinHeapAlignedReallocImpl(void* ptr,
                                       size_t size,
                                       size_t alignment,
                                       AllocToken,
                                       void* context) {
  return allocator_shim::WinHeapAlignedRealloc(ptr, size, alignment);
}

void DefaultWinHeapAlignedFreeImpl(void* ptr, void* context) {
  allocator_shim::WinHeapAlignedFree(ptr);
}

}  // namespace

// Guarantee that default_dispatch is compile-time initialized to avoid using
// it before initialization (allocations before main in release builds with
// optimizations disabled).
constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &DefaultWinHeapMallocImpl,
    &DefaultWinHeapMallocImpl, /* alloc_unchecked_function */
    &DefaultWinHeapCallocImpl,
    &DefaultWinHeapCallocImpl, /* alloc_zero_initialized_unchecked_function */
    &DefaultWinHeapMemalignImpl,
    &DefaultWinHeapReallocImpl,
    &DefaultWinHeapReallocImpl, /* realloc_unchecked_function */
    &DefaultWinHeapFreeImpl,
    &DefaultWinHeapFreeWithSizeImpl,
    &DefaultWinHeapFreeWithAlignmentImpl,
    &DefaultWinHeapFreeWithSizeAndAlignmentImpl,
    &DefaultWinHeapGetSizeEstimateImpl,
    nullptr, /* good_size */
    nullptr, /* claimed_address */
    nullptr, /* batch_malloc_function */
    nullptr, /* batch_free_function */
    nullptr, /* try_free_default_function */
    &DefaultWinHeapAlignedMallocImpl,
    &DefaultWinHeapAlignedMallocImpl, /* aligned_malloc_unchecked_function */
    &DefaultWinHeapAlignedReallocImpl,
    &DefaultWinHeapAlignedReallocImpl, /* aligned_realloc_unchecked_function */
    &DefaultWinHeapAlignedFreeImpl,
    nullptr, /* next */
};
