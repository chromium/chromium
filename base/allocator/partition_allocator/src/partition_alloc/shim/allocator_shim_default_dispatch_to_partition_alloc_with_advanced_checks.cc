// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_with_advanced_checks.h"

#include <atomic>

#include "partition_alloc/partition_alloc_base/check.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/shim/allocator_dispatch.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc_internal.h"

namespace allocator_shim {

namespace {
std::atomic<const AllocatorDispatch*> g_delegate_dispatch =
    &internal::kPartitionAllocDispatch;

PA_ALWAYS_INLINE const AllocatorDispatch* GetDelegate() {
  return g_delegate_dispatch.load(std::memory_order_relaxed);
}

void* DelegatedAllocFn(size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_function(size, context);
}

void* DelegatedAllocUncheckedFn(size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_unchecked_function(size, context);
}

void* DelegatedAllocZeroInitializedFn(size_t n, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_zero_initialized_function(n, size,
                                                               context);
}

void* DelegatedAllocAlignedFn(size_t alignment, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->alloc_aligned_function(alignment, size, context);
}

void* DelegatedReallocFn(void* address, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->realloc_function(address, size, context);
}

void* DelegatedReallocUncheckedFn(void* address, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->realloc_unchecked_function(address, size,
                                                          context);
}

void DelegatedFreeFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_function(address, context);
}

size_t DelegatedGetSizeEstimateFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->get_size_estimate_function(address, context);
}

size_t DelegatedGoodSizeFn(size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->good_size_function(size, context);
}

bool DelegatedClaimedAddressFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->claimed_address_function(address, context);
}

unsigned DelegatedBatchMallocFn(size_t size,
                                void** results,
                                unsigned num_requested,
                                void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->batch_malloc_function(size, results,
                                                     num_requested, context);
}

void DelegatedBatchFreeFn(void** to_be_freed,
                          unsigned num_to_be_freed,
                          void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->batch_free_function(to_be_freed, num_to_be_freed,
                                                   context);
}

void DelegatedFreeDefiniteSizeFn(void* address, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_definite_size_function(address, size,
                                                           context);
}

void DelegatedTryFreeDefaultFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->try_free_default_function(address, context);
}

void* DelegatedAlignedMallocFn(size_t size, size_t alignment, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_malloc_function(size, alignment,
                                                       context);
}

void* DelegatedAlignedMallocUncheckedFn(size_t size,
                                        size_t alignment,
                                        void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_malloc_unchecked_function(
      size, alignment, context);
}

void* DelegatedAlignedReallocFn(void* address,
                                size_t size,
                                size_t alignment,
                                void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_realloc_function(address, size,
                                                        alignment, context);
}

void* DelegatedAlignedReallocUncheckedFn(void* address,
                                         size_t size,
                                         size_t alignment,
                                         void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_realloc_unchecked_function(
      address, size, alignment, context);
}

void DelegatedAlignedFreeFn(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->aligned_free_function(address, context);
}
}  // namespace

void InstallCustomDispatch(AllocatorDispatch* dispatch) {
  PA_DCHECK(dispatch);

  // Must have followings:
  PA_DCHECK(dispatch->alloc_function != nullptr);
  PA_DCHECK(dispatch->alloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->alloc_zero_initialized_function != nullptr);
  PA_DCHECK(dispatch->alloc_aligned_function != nullptr);
  PA_DCHECK(dispatch->realloc_function != nullptr);
  PA_DCHECK(dispatch->realloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->free_function != nullptr);
  PA_DCHECK(dispatch->get_size_estimate_function != nullptr);
#if PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->good_size_function != nullptr);
  PA_DCHECK(dispatch->claimed_address_function != nullptr);
#endif  // PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->batch_malloc_function != nullptr);
  PA_DCHECK(dispatch->batch_free_function != nullptr);
#if PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->free_definite_size_function != nullptr);
  PA_DCHECK(dispatch->try_free_default_function != nullptr);
#endif  // PA_BUILDFLAG(IS_APPLE)
  PA_DCHECK(dispatch->aligned_malloc_function != nullptr);
  PA_DCHECK(dispatch->aligned_malloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->aligned_realloc_function != nullptr);
  PA_DCHECK(dispatch->aligned_realloc_unchecked_function != nullptr);
  PA_DCHECK(dispatch->aligned_free_function != nullptr);

  dispatch->next = &internal::kPartitionAllocDispatch;

  // Unlike `InsertAllocatorDispatch(...)`, we don't have any invariant here.
  // Hence using relaxed memory ordering.
#if !PA_BUILDFLAG(DCHECKS_ARE_ON)
  g_delegate_dispatch.store(dispatch, std::memory_order_relaxed);
#else
  const AllocatorDispatch* previous_value =
      g_delegate_dispatch.exchange(dispatch, std::memory_order_relaxed);
  // We also allow `previous_value == dispatch` i.e. `dispatch` is written
  // twice - sometimes it is hard to guarantee "exactly once" initialization.
  PA_DCHECK(previous_value == &internal::kPartitionAllocDispatch ||
            previous_value == dispatch);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
}

void InstallCustomDispatchForTesting(AllocatorDispatch* dispatch) {
  InstallCustomDispatch(dispatch);
}

void InstallCustomDispatchForPartitionAllocWithAdvancedChecks() {
  PA_CONSTINIT static AllocatorDispatch dispatch = []() constexpr {
    auto dispatch =
        internal::PartitionAllocWithAdvancedChecksFunctions::MakeDispatch();
    dispatch.next = &internal::kPartitionAllocDispatch;
    return dispatch;
  }();
  InstallCustomDispatch(&dispatch);
}

void UninstallCustomDispatch() {
  g_delegate_dispatch.store(&internal::kPartitionAllocDispatch,
                            std::memory_order_relaxed);
}

const AllocatorDispatch AllocatorDispatch::default_dispatch = {
    .alloc_function = &DelegatedAllocFn,
    .alloc_unchecked_function = &DelegatedAllocUncheckedFn,
    .alloc_zero_initialized_function = &DelegatedAllocZeroInitializedFn,
    .alloc_aligned_function = &DelegatedAllocAlignedFn,
    .realloc_function = &DelegatedReallocFn,
    .realloc_unchecked_function = &DelegatedReallocUncheckedFn,
    .free_function = &DelegatedFreeFn,
    .get_size_estimate_function = &DelegatedGetSizeEstimateFn,
    .good_size_function = &DelegatedGoodSizeFn,
    .claimed_address_function = &DelegatedClaimedAddressFn,
    .batch_malloc_function = &DelegatedBatchMallocFn,
    .batch_free_function = &DelegatedBatchFreeFn,
    .free_definite_size_function = &DelegatedFreeDefiniteSizeFn,
    .try_free_default_function = &DelegatedTryFreeDefaultFn,
    .aligned_malloc_function = &DelegatedAlignedMallocFn,
    .aligned_malloc_unchecked_function = &DelegatedAlignedMallocUncheckedFn,
    .aligned_realloc_function = &DelegatedAlignedReallocFn,
    .aligned_realloc_unchecked_function = &DelegatedAlignedReallocUncheckedFn,
    .aligned_free_function = &DelegatedAlignedFreeFn,
    .next = nullptr,
};

}  // namespace allocator_shim
