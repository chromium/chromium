// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_hooks.h"

#include <ostream>

#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc {

namespace {

internal::Lock g_hook_lock;

internal::Lock& GetHooksLock() {
  return g_hook_lock;
}

}  // namespace

std::atomic<bool> PartitionAllocHooks::hooks_enabled_(false);
std::atomic<PartitionAllocHooks::AllocationObserverHook*>
    PartitionAllocHooks::allocation_observer_hook_(nullptr);
std::atomic<PartitionAllocHooks::FreeObserverHook*>
    PartitionAllocHooks::free_observer_hook_(nullptr);
std::atomic<PartitionAllocHooks::AllocationOverrideHook*>
    PartitionAllocHooks::allocation_override_hook_(nullptr);
std::atomic<PartitionAllocHooks::FreeOverrideHook*>
    PartitionAllocHooks::free_override_hook_(nullptr);
std::atomic<PartitionAllocHooks::ReallocOverrideHook*>
    PartitionAllocHooks::realloc_override_hook_(nullptr);
std::atomic<PartitionAllocHooks::QuarantineOverrideHook*>
    PartitionAllocHooks::quarantine_override_hook_(nullptr);

void PartitionAllocHooks::SetObserverHooks(AllocationObserverHook* alloc_hook,
                                           FreeObserverHook* free_hook) {
  internal::ScopedGuard guard(GetHooksLock());

  // Chained hooks are not supported. Registering a non-null hook when a
  // non-null hook is already registered indicates somebody is trying to
  // overwrite a hook.
  PA_CHECK((!allocation_observer_hook_ && !free_observer_hook_) ||
           (!alloc_hook && !free_hook))
      << "Overwriting already set observer hooks";
  allocation_observer_hook_ = alloc_hook;
  free_observer_hook_ = free_hook;

  hooks_enabled_ = allocation_observer_hook_ || allocation_override_hook_;
}

void PartitionAllocHooks::SetOverrideHooks(AllocationOverrideHook* alloc_hook,
                                           FreeOverrideHook* free_hook,
                                           ReallocOverrideHook realloc_hook) {
  internal::ScopedGuard guard(GetHooksLock());

  PA_CHECK((!allocation_override_hook_ && !free_override_hook_ &&
            !realloc_override_hook_) ||
           (!alloc_hook && !free_hook && !realloc_hook))
      << "Overwriting already set override hooks";
  allocation_override_hook_ = alloc_hook;
  free_override_hook_ = free_hook;
  realloc_override_hook_ = realloc_hook;

  hooks_enabled_ = allocation_observer_hook_ || allocation_override_hook_;
}

void PartitionAllocHooks::AllocationObserverHookIfEnabled(
    const partition_alloc::AllocationNotificationData& notification_data) {
  if (auto* hook = allocation_observer_hook_.load(std::memory_order_relaxed)) {
    hook(notification_data);
  }
}

bool PartitionAllocHooks::AllocationOverrideHookIfEnabled(
    void** out,
    AllocFlags flags,
    size_t size,
    const char* type_name) {
  if (auto* hook = allocation_override_hook_.load(std::memory_order_relaxed)) {
    return hook(out, flags, size, type_name);
  }
  return false;
}

void PartitionAllocHooks::FreeObserverHookIfEnabled(
    const FreeNotificationData& notification_data) {
  if (auto* hook = free_observer_hook_.load(std::memory_order_relaxed)) {
    hook(notification_data);
  }
}

bool PartitionAllocHooks::FreeOverrideHookIfEnabled(void* address) {
  if (auto* hook = free_override_hook_.load(std::memory_order_relaxed)) {
    return hook(address);
  }
  return false;
}

void PartitionAllocHooks::ReallocObserverHookIfEnabled(
    const FreeNotificationData& free_notification_data,
    const AllocationNotificationData& allocation_notification_data) {
  // Report a reallocation as a free followed by an allocation.
  AllocationObserverHook* allocation_hook =
      allocation_observer_hook_.load(std::memory_order_relaxed);
  FreeObserverHook* free_hook =
      free_observer_hook_.load(std::memory_order_relaxed);
  if (allocation_hook && free_hook) {
    free_hook(free_notification_data);
    allocation_hook(allocation_notification_data);
  }
}

bool PartitionAllocHooks::ReallocOverrideHookIfEnabled(size_t* out,
                                                       void* address) {
  if (ReallocOverrideHook* hook =
          realloc_override_hook_.load(std::memory_order_relaxed)) {
    return hook(out, address);
  }
  return false;
}

// Do not unset the hook if there are remaining quarantined slots
// not to break checks on unquarantining.
void PartitionAllocHooks::SetQuarantineOverrideHook(
    QuarantineOverrideHook* hook) {
  quarantine_override_hook_.store(hook, std::memory_order_release);
}

}  // namespace partition_alloc
