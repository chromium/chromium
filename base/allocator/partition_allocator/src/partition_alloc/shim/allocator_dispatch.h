// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_DISPATCH_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_DISPATCH_H_

#include <cstddef>

namespace allocator_shim {

struct AllocatorDispatch {
  using AllocFn = void*(const AllocatorDispatch* self,
                        size_t size,
                        void* context);
  using AllocUncheckedFn = void*(const AllocatorDispatch* self,
                                 size_t size,
                                 void* context);
  using AllocZeroInitializedFn = void*(const AllocatorDispatch* self,
                                       size_t n,
                                       size_t size,
                                       void* context);
  using AllocAlignedFn = void*(const AllocatorDispatch* self,
                               size_t alignment,
                               size_t size,
                               void* context);
  using ReallocFn = void*(const AllocatorDispatch* self,
                          void* address,
                          size_t size,
                          void* context);
  using FreeFn = void(const AllocatorDispatch* self,
                      void* address,
                      void* context);
  // Returns the allocated size of user data (not including heap overhead).
  // Can be larger than the requested size.
  using GetSizeEstimateFn = size_t(const AllocatorDispatch* self,
                                   void* address,
                                   void* context);
  using GoodSizeFn = size_t(const AllocatorDispatch* self,
                            size_t size,
                            void* context);
  using ClaimedAddressFn = bool(const AllocatorDispatch* self,
                                void* address,
                                void* context);
  using BatchMallocFn = unsigned(const AllocatorDispatch* self,
                                 size_t size,
                                 void** results,
                                 unsigned num_requested,
                                 void* context);
  using BatchFreeFn = void(const AllocatorDispatch* self,
                           void** to_be_freed,
                           unsigned num_to_be_freed,
                           void* context);
  using FreeDefiniteSizeFn = void(const AllocatorDispatch* self,
                                  void* ptr,
                                  size_t size,
                                  void* context);
  using TryFreeDefaultFn = void(const AllocatorDispatch* self,
                                void* ptr,
                                void* context);
  using AlignedMallocFn = void*(const AllocatorDispatch* self,
                                size_t size,
                                size_t alignment,
                                void* context);
  using AlignedReallocFn = void*(const AllocatorDispatch* self,
                                 void* address,
                                 size_t size,
                                 size_t alignment,
                                 void* context);
  using AlignedFreeFn = void(const AllocatorDispatch* self,
                             void* address,
                             void* context);

  AllocFn* const alloc_function;
  AllocUncheckedFn* const alloc_unchecked_function;
  AllocZeroInitializedFn* const alloc_zero_initialized_function;
  AllocAlignedFn* const alloc_aligned_function;
  ReallocFn* const realloc_function;
  FreeFn* const free_function;
  GetSizeEstimateFn* const get_size_estimate_function;
  GoodSizeFn* const good_size_function;
  // claimed_address, batch_malloc, batch_free, free_definite_size and
  // try_free_default are specific to the OSX and iOS allocators.
  ClaimedAddressFn* const claimed_address_function;
  BatchMallocFn* const batch_malloc_function;
  BatchFreeFn* const batch_free_function;
  FreeDefiniteSizeFn* const free_definite_size_function;
  TryFreeDefaultFn* const try_free_default_function;
  // _aligned_malloc, _aligned_realloc, and _aligned_free are specific to the
  // Windows allocator.
  AlignedMallocFn* const aligned_malloc_function;
  AlignedReallocFn* const aligned_realloc_function;
  AlignedFreeFn* const aligned_free_function;

  const AllocatorDispatch* next;

  // |default_dispatch| is statically defined by one (and only one) of the
  // allocator_shim_default_dispatch_to_*.cc files, depending on the build
  // configuration.
  static const AllocatorDispatch default_dispatch;
};

}  // namespace allocator_shim

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_DISPATCH_H_
