// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_HOOKS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_HOOKS_H_

#include <atomic>
#include <cstddef>

#include "base/base_export.h"

namespace base {

// PartitionAlloc supports setting hooks to observe allocations/frees as they
// occur as well as 'override' hooks that allow overriding those operations.
class BASE_EXPORT PartitionAllocHooks {
 public:
  // Log allocation and free events.
  typedef void AllocationObserverHook(void* address,
                                      size_t size,
                                      const char* type_name);
  typedef void FreeObserverHook(void* address);

  // If it returns true, the allocation has been overridden with the pointer in
  // *out.
  typedef bool AllocationOverrideHook(void** out,
                                      int flags,
                                      size_t size,
                                      const char* type_name);
  // If it returns true, then the allocation was overridden and has been freed.
  typedef bool FreeOverrideHook(void* address);
  // If it returns true, the underlying allocation is overridden and *out holds
  // the size of the underlying allocation.
  typedef bool ReallocOverrideHook(size_t* out, void* address);

  // To unhook, call Set*Hooks with nullptrs.
  static void SetObserverHooks(AllocationObserverHook* alloc_hook,
                               FreeObserverHook* free_hook);
  static void SetOverrideHooks(AllocationOverrideHook* alloc_hook,
                               FreeOverrideHook* free_hook,
                               ReallocOverrideHook realloc_hook);

  // Helper method to check whether hooks are enabled. This is an optimization
  // so that if a function needs to call observer and override hooks in two
  // different places this value can be cached and only loaded once.
  static bool AreHooksEnabled() {
    return hooks_enabled_.load(std::memory_order_relaxed);
  }

  static void AllocationObserverHookIfEnabled(void* address,
                                              size_t size,
                                              const char* type_name);
  static bool AllocationOverrideHookIfEnabled(void** out,
                                              int flags,
                                              size_t size,
                                              const char* type_name);

  static void FreeObserverHookIfEnabled(void* address);
  static bool FreeOverrideHookIfEnabled(void* address);

  static void ReallocObserverHookIfEnabled(void* old_address,
                                           void* new_address,
                                           size_t size,
                                           const char* type_name);
  static bool ReallocOverrideHookIfEnabled(size_t* out, void* address);

 private:
  // Single bool that is used to indicate whether observer or allocation hooks
  // are set to reduce the numbers of loads required to check whether hooking is
  // enabled.
  static std::atomic<bool> hooks_enabled_;

  // Lock used to synchronize Set*Hooks calls.
  static std::atomic<AllocationObserverHook*> allocation_observer_hook_;
  static std::atomic<FreeObserverHook*> free_observer_hook_;

  static std::atomic<AllocationOverrideHook*> allocation_override_hook_;
  static std::atomic<FreeOverrideHook*> free_override_hook_;
  static std::atomic<ReallocOverrideHook*> realloc_override_hook_;
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_HOOKS_H_
