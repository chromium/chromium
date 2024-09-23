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
}  // namespace

void InstallDispatchToPartitionAllocWithAdvancedChecks(
    AllocatorDispatch* dispatch) {
  PA_DCHECK(dispatch);

  // Must have followings:
  PA_DCHECK(dispatch->realloc_function != nullptr);
  PA_DCHECK(dispatch->free_function != nullptr);

  // Must not have followings:
  PA_DCHECK(dispatch->alloc_function == nullptr);
  PA_DCHECK(dispatch->alloc_unchecked_function == nullptr);
  PA_DCHECK(dispatch->alloc_zero_initialized_function == nullptr);
  PA_DCHECK(dispatch->alloc_aligned_function == nullptr);
  PA_DCHECK(dispatch->realloc_unchecked_function == nullptr);
  PA_DCHECK(dispatch->get_size_estimate_function == nullptr);
  PA_DCHECK(dispatch->good_size_function == nullptr);
  PA_DCHECK(dispatch->claimed_address_function == nullptr);
  PA_DCHECK(dispatch->batch_malloc_function == nullptr);
  PA_DCHECK(dispatch->batch_free_function == nullptr);
  PA_DCHECK(dispatch->free_definite_size_function == nullptr);
  PA_DCHECK(dispatch->try_free_default_function == nullptr);
  PA_DCHECK(dispatch->aligned_malloc_function == nullptr);
  PA_DCHECK(dispatch->aligned_malloc_unchecked_function == nullptr);
  PA_DCHECK(dispatch->aligned_realloc_function == nullptr);
  PA_DCHECK(dispatch->aligned_realloc_unchecked_function == nullptr);
  PA_DCHECK(dispatch->aligned_free_function == nullptr);

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

void UninstallDispatchToPartitionAllocWithAdvancedChecks() {
  g_delegate_dispatch.store(&internal::kPartitionAllocDispatch,
                            std::memory_order_relaxed);
}

namespace internal {

void FreeWithAdvancedChecks(void* address, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->free_function(address, context);
}

void* ReallocWithAdvancedChecks(void* address, size_t size, void* context) {
  const AllocatorDispatch* delegate = GetDelegate();
  PA_MUSTTAIL return delegate->realloc_function(address, size, context);
}

}  // namespace internal

const AllocatorDispatch AllocatorDispatch::default_dispatch = []() constexpr {
  AllocatorDispatch dispatch = internal::kPartitionAllocDispatch;
  dispatch.realloc_function = &internal::ReallocWithAdvancedChecks;
  dispatch.free_function = &internal::FreeWithAdvancedChecks;
  return dispatch;
}();

}  // namespace allocator_shim
