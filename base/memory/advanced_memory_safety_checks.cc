// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/advanced_memory_safety_checks.h"

#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// We cannot hide things behind anonymous namespace because they are referenced
// via macro, which can be defined anywhere.
// To avoid tainting ::base namespace, define things inside this namespace.
namespace base::internal {

// Define type traits to determine type |T|'s memory safety check status.
namespace {

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

// Allocator type traits.
constexpr bool ShouldUsePartitionAlloc(MemorySafetyCheck checks) {
  return static_cast<bool>(checks &
                           (MemorySafetyCheck::kForcePartitionAlloc |
                            MemorySafetyCheck::kSchedulerLoopQuarantine));
}

// Returns |partition_alloc::AllocFlags| corresponding to |checks|.
constexpr partition_alloc::AllocFlags GetAllocFlags(MemorySafetyCheck checks) {
  return partition_alloc::AllocFlags::kReturnNull |
         partition_alloc::AllocFlags::kNoHooks;
}

// Returns |partition_alloc::FreeFlags| corresponding to |checks|.
constexpr partition_alloc::FreeFlags GetFreeFlags(MemorySafetyCheck checks) {
  auto flags = partition_alloc::FreeFlags::kNone;
  if (static_cast<bool>(checks & MemorySafetyCheck::kSchedulerLoopQuarantine)) {
    flags |= partition_alloc::FreeFlags::
        kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks;
  }
  return flags;
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

// Allocator functions.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
ALWAYS_INLINE partition_alloc::PartitionRoot*
GetPartitionRootForMemorySafetyCheckedAllocation() {
  return allocator_shim::internal::PartitionAllocMalloc::Allocator();
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

template <MemorySafetyCheck checks>
NOINLINE void* HandleMemorySafetyCheckedOperatorNew(std::size_t count) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if constexpr (ShouldUsePartitionAlloc(checks)) {
    return GetPartitionRootForMemorySafetyCheckedAllocation()
        ->AllocInline<GetAllocFlags(checks)>(count);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return ::operator new(count);
}

template <MemorySafetyCheck checks>
NOINLINE void* HandleMemorySafetyCheckedOperatorNew(
    std::size_t count,
    std::align_val_t alignment) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if constexpr (ShouldUsePartitionAlloc(checks)) {
    return GetPartitionRootForMemorySafetyCheckedAllocation()
        ->AlignedAlloc<GetAllocFlags(checks)>(static_cast<size_t>(alignment),
                                              count);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  return ::operator new(count, alignment);
}

template <MemorySafetyCheck checks>
NOINLINE void HandleMemorySafetyCheckedOperatorDelete(void* ptr) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if constexpr (ShouldUsePartitionAlloc(checks)) {
    GetPartitionRootForMemorySafetyCheckedAllocation()
        ->Free<GetFreeFlags(checks)>(ptr);
    return;
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ::operator delete(ptr);
}

template <MemorySafetyCheck checks>
NOINLINE void HandleMemorySafetyCheckedOperatorDelete(
    void* ptr,
    std::align_val_t alignment) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  if constexpr (ShouldUsePartitionAlloc(checks)) {
    GetPartitionRootForMemorySafetyCheckedAllocation()
        ->Free<GetFreeFlags(checks)>(ptr);
    return;
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ::operator delete(ptr, alignment);
}

FOR_EACH_BASE_INTERNAL_MEMORY_SAFETY_CHECK_VALUE(
    DEFINE_BASE_INTERNAL_HANDLE_MEMORY_SAFETY_CHECKED_OPERATORS)

}  // namespace base::internal
