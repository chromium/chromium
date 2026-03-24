// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_ASAN_ALLOCATOR_H_
#define BASE_MEMORY_RAW_PTR_ASAN_ALLOCATOR_H_

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#include "partition_alloc/partition_alloc.h"

namespace base::internal {

partition_alloc::PartitionRoot& GetRawPtrAsanInternalAllocator();

template <typename T>
struct RawPtrAsanAllocator {
  using value_type = T;
  using is_always_equal = std::true_type;

  constexpr RawPtrAsanAllocator() noexcept = default;
  template <typename U>
  constexpr RawPtrAsanAllocator(const RawPtrAsanAllocator<U>& other) noexcept {}

  static constexpr partition_alloc::AllocFlags alloc_flags =
      partition_alloc::AllocFlags::kNoHooks |
      partition_alloc::AllocFlags::kNoMemoryToolOverride |
      partition_alloc::AllocFlags::kNoOverrideHooks;
  static constexpr partition_alloc::FreeFlags free_flags =
      partition_alloc::FreeFlags::kNoHooks |
      partition_alloc::FreeFlags::kNoMemoryToolOverride;

  value_type* allocate(std::size_t count) {
    PA_CHECK(count <=
             std::numeric_limits<std::size_t>::max() / sizeof(value_type));
    return static_cast<value_type*>(
        GetRawPtrAsanInternalAllocator().Alloc<alloc_flags>(
            count * sizeof(value_type)));
  }

  void deallocate(value_type* p, [[maybe_unused]] std::size_t n) {
    GetRawPtrAsanInternalAllocator().Free<free_flags>(p);
  }
};

}  // namespace base::internal

#endif  // PA_BUILDFLAG(USE_ASAN_BACKUP_REF_PTR_V2)

#endif  // BASE_MEMORY_RAW_PTR_ASAN_ALLOCATOR_H_
