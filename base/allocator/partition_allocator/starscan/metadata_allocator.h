// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_

#include <utility>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_root.h"

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PartitionRoot& PCScanMetadataAllocator();
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
    return static_cast<value_type*>(
        PCScanMetadataAllocator()
            .AllocInline<partition_alloc::AllocFlags::kNoHooks>(
                size * sizeof(value_type)));
  }

  void deallocate(value_type* ptr, size_t size) {
    PCScanMetadataAllocator().FreeInline<FreeFlags::kNoHooks>(ptr);
  }
};

// Inherit from it to make a class allocated on the metadata partition.
struct AllocatedOnPCScanMetadataPartition {
  static void* operator new(size_t size) {
    return PCScanMetadataAllocator()
        .AllocInline<partition_alloc::AllocFlags::kNoHooks>(size);
  }
  static void operator delete(void* ptr) {
    PCScanMetadataAllocator().FreeInline<FreeFlags::kNoHooks>(ptr);
  }
};

template <typename T, typename... Args>
T* MakePCScanMetadata(Args&&... args) {
  auto* memory = static_cast<T*>(
      PCScanMetadataAllocator()
          .AllocInline<partition_alloc::AllocFlags::kNoHooks>(sizeof(T)));
  return new (memory) T(std::forward<Args>(args)...);
}

struct PCScanMetadataDeleter final {
  inline void operator()(void* ptr) const {
    PCScanMetadataAllocator().FreeInline<FreeFlags::kNoHooks>(ptr);
  }
};

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_STARSCAN_METADATA_ALLOCATOR_H_
