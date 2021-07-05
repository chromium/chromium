// Copyright (c) 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"

namespace base {
namespace internal {

ThreadSafePartitionRoot& PCScanMetadataAllocator();
void ReinitPCScanMetadataAllocatorForTesting();

// STL allocator which is needed to keep internal data structures required by
// PCScan.
template <typename T>
class MetadataAllocator {
 public:
  using value_type = T;

  MetadataAllocator() = default;

  template <typename U>
  MetadataAllocator(const MetadataAllocator<U>&) {}  // NOLINT

  template <typename U>
  MetadataAllocator& operator=(const MetadataAllocator<U>&) {
    return *this;
  }

  template <typename U>
  bool operator==(const MetadataAllocator<U>&) {
    return true;
  }

  template <typename U>
  bool operator!=(const MetadataAllocator<U>& o) {
    return !operator==(o);
  }

  value_type* allocate(size_t size) {
    return static_cast<value_type*>(PCScanMetadataAllocator().AllocFlagsNoHooks(
        0, size * sizeof(value_type), PartitionPageSize()));
  }

  void deallocate(value_type* ptr, size_t size) {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }
};

// Inherit from it to make a class allocated on the metadata partition.
struct AllocatedOnPCScanMetadataPartition {
  static void* operator new(size_t size) {
    return PCScanMetadataAllocator().AllocFlagsNoHooks(0, size,
                                                       PartitionPageSize());
  }
  static void operator delete(void* ptr) {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }
};

template <typename T, typename... Args>
T* MakePCScanMetadata(Args&&... args) {
  auto* memory = static_cast<T*>(PCScanMetadataAllocator().AllocFlagsNoHooks(
      0, sizeof(T), PartitionPageSize()));
  return new (memory) T(std::forward<Args>(args)...);
}

struct PCScanMetadataDeleter final {
  inline void operator()(void* ptr) const {
    PCScanMetadataAllocator().FreeNoHooks(ptr);
  }
};

}  // namespace internal
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_
