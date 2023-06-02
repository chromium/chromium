// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_TAGGING_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_TAGGING_H_

// This file contains method definitions to support Armv8.5-A's memory tagging
// extension.

#include <cstddef>
#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "build/build_config.h"

namespace partition_alloc {

// Enum configures Arm's MTE extension to operate in different modes
enum class TagViolationReportingMode {
  // Default settings
  kUndefined,
  // MTE explicitly disabled.
  kDisabled,
  // Precise tag violation reports, higher overhead. Good for unittests
  // and security critical threads.
  kSynchronous,
  // Imprecise tag violation reports (async mode). Lower overhead.
  kAsynchronous,
};

// Changes the memory tagging mode for the calling thread.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void ChangeMemoryTaggingModeForCurrentThread(TagViolationReportingMode);

namespace internal {

constexpr int kMemTagGranuleSize = 16u;
#if PA_CONFIG(HAS_MEMORY_TAGGING)
constexpr uint64_t kPtrTagMask = 0xff00000000000000uLL;
#else
constexpr uint64_t kPtrTagMask = 0;
#endif  // PA_CONFIG(HAS_MEMORY_TAGGING)
constexpr uint64_t kPtrUntagMask = ~kPtrTagMask;

#if BUILDFLAG(IS_ANDROID)
// Changes the memory tagging mode for all threads in the current process.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void ChangeMemoryTaggingModeForAllThreadsPerProcess(TagViolationReportingMode);
#endif

// Gets the memory tagging mode for the calling thread. Returns kUndefined if
// MTE support is not available.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
TagViolationReportingMode GetMemoryTaggingModeForCurrentThread();

// These forward-defined functions do not really exist in tagging.cc, they're resolved
// by the dynamic linker to MTE-capable versions on the right hardware.
#if PA_CONFIG(HAS_MEMORY_TAGGING)
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* TagMemoryRangeIncrementInternal(void* ptr, size_t size);
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* TagMemoryRangeRandomlyInternal(void* ptr, size_t size, uint64_t mask);
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void* RemaskPointerInternal(void* ptr);
#endif

// Increments the tag of the memory range ptr. Useful for provable revocations
// (e.g. free). Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
// TODO(bartekn): Remove the T* variant.
// TODO(bartekn): Consider removing the return value.
template <typename T>
PA_ALWAYS_INLINE T* TagMemoryRangeIncrement(T* ptr, size_t size) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(TagMemoryRangeIncrementInternal(ptr, size));
#else
  return ptr;
#endif
}
PA_ALWAYS_INLINE void* TagMemoryRangeIncrement(uintptr_t ptr, size_t size) {
  return TagMemoryRangeIncrement(reinterpret_cast<void*>(ptr), size);
}

// Randomly changes the tag of the ptr memory range. Useful for initial random
// initialization. Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
// TODO(bartekn): Remove the T* variant.
template <typename T>
PA_ALWAYS_INLINE T* TagMemoryRangeRandomly(T* ptr,
                                           size_t size,
                                           uint64_t mask = 0u) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(TagMemoryRangeRandomlyInternal(ptr, size, mask));
#else
  return ptr;
#endif
}
PA_ALWAYS_INLINE void* TagMemoryRangeRandomly(uintptr_t ptr,
                                              size_t size,
                                              uint64_t mask = 0u) {
  return TagMemoryRangeRandomly(reinterpret_cast<void*>(ptr), size, mask);
}

// Gets a version of ptr that's safe to dereference.
template <typename T>
PA_ALWAYS_INLINE T* TagPtr(T* ptr) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(RemaskPointerInternal(ptr));
#else
  return ptr;
#endif
}

// Gets a version of |address| that's safe to dereference, and casts to a
// pointer.
PA_ALWAYS_INLINE void* TagAddr(uintptr_t address) {
  return TagPtr(reinterpret_cast<void*>(address));
}

// Strips the tag bits off |address|.
PA_ALWAYS_INLINE uintptr_t UntagAddr(uintptr_t address) {
#if PA_CONFIG(HAS_MEMORY_TAGGING)
  return address & internal::kPtrUntagMask;
#else
  return address;
#endif
}

}  // namespace internal

// Strips the tag bits off |ptr|.
template <typename T>
PA_ALWAYS_INLINE uintptr_t UntagPtr(T* ptr) {
  return internal::UntagAddr(reinterpret_cast<uintptr_t>(ptr));
}

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_TAGGING_H_
