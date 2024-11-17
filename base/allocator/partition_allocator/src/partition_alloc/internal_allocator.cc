// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/internal_allocator.h"

namespace partition_alloc::internal {
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PartitionRoot& InternalAllocatorRoot() {
  static internal::base::NoDestructor<PartitionRoot> allocator([] {
    // Disable features using the internal root to avoid reentrancy issue.
    PartitionOptions opts;
    opts.thread_cache = PartitionOptions::kDisabled;
    opts.scheduler_loop_quarantine = PartitionOptions::kDisabled;
    return opts;
  }());

  return *allocator;
}

// static
void* InternalPartitionAllocated::operator new(size_t count) {
  return InternalAllocatorRoot().Alloc<AllocFlags::kNoHooks>(count);
}
// static
void* InternalPartitionAllocated::operator new(size_t count,
                                               std::align_val_t alignment) {
  return InternalAllocatorRoot().AlignedAlloc<AllocFlags::kNoHooks>(
      static_cast<size_t>(alignment), count);
}
// static
void InternalPartitionAllocated::operator delete(void* ptr) {
  InternalAllocatorRoot().Free<FreeFlags::kNoHooks>(ptr);
}
// static
void InternalPartitionAllocated::operator delete(void* ptr, std::align_val_t) {
  InternalAllocatorRoot().Free<FreeFlags::kNoHooks>(ptr);
}

// A deleter for `std::unique_ptr<T>`.
void InternalPartitionDeleter::operator()(void* ptr) const {
  InternalAllocatorRoot().Free<FreeFlags::kNoHooks>(ptr);
}
}  // namespace partition_alloc::internal
