// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/dispatcher/internal/dispatch_data.h"

#include "partition_alloc/buildflags.h"

namespace base::allocator::dispatcher::internal {

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)

DispatchData& DispatchData::SetAllocationObserverHooks(
    AllocationObserverHook* allocation_observer_hook,
    FreeObserverHook* free_observer_hook) {
  allocation_observer_hook_ = allocation_observer_hook;
  free_observer_hook_ = free_observer_hook;

  return *this;
}

DispatchData::AllocationObserverHook* DispatchData::GetAllocationObserverHook()
    const {
  return allocation_observer_hook_;
}

DispatchData::FreeObserverHook* DispatchData::GetFreeObserverHook() const {
  return free_observer_hook_;
}
#endif

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
DispatchData& DispatchData::SetAllocatorDispatch(
    AllocatorDispatch* allocator_dispatch) {
  allocator_dispatch_ = allocator_dispatch;
  return *this;
}

AllocatorDispatch* DispatchData::GetAllocatorDispatch() const {
  return allocator_dispatch_;
}
#endif
}  // namespace base::allocator::dispatcher::internal
