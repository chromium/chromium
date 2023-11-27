// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCH_DATA_H_
#define BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCH_DATA_H_

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"
#include "base/base_export.h"
#include "build/build_config.h"

#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_hooks.h"
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "base/allocator/partition_allocator/src/partition_alloc/shim/allocator_shim.h"
#endif

namespace base::allocator::dispatcher::internal {

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
using allocator_shim::AllocatorDispatch;
#endif

// A simple utility class to pass all the information required to properly hook
// into the memory allocation subsystems from DispatcherImpl to the Dispatcher.
struct BASE_EXPORT DispatchData {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  using AllocationObserverHook =
      partition_alloc::PartitionAllocHooks::AllocationObserverHook;
  using FreeObserverHook =
      partition_alloc::PartitionAllocHooks::FreeObserverHook;

  DispatchData& SetAllocationObserverHooks(AllocationObserverHook*,
                                           FreeObserverHook*);
  AllocationObserverHook* GetAllocationObserverHook() const;
  FreeObserverHook* GetFreeObserverHook() const;

 private:
  AllocationObserverHook* allocation_observer_hook_ = nullptr;
  FreeObserverHook* free_observer_hook_ = nullptr;

 public:
#endif

#if BUILDFLAG(USE_ALLOCATOR_SHIM)
  DispatchData& SetAllocatorDispatch(AllocatorDispatch* allocator_dispatch);
  AllocatorDispatch* GetAllocatorDispatch() const;

 private:
  AllocatorDispatch* allocator_dispatch_ = nullptr;
#endif
};

}  // namespace base::allocator::dispatcher::internal

#endif  // BASE_ALLOCATOR_DISPATCHER_INTERNAL_DISPATCH_DATA_H_
