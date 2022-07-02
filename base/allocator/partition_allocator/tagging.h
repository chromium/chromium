// Copyright (c) 2021 The Chromium Authors. All rights reserved.
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
#if defined(PA_HAS_MEMORY_TAGGING)
constexpr uint64_t kMemTagUnmask = 0x00ffffffffffffffuLL;
#else
constexpr uint64_t kMemTagUnmask = 0xffffffffffffffffuLL;
#endif  // defined(PA_HAS_MEMORY_TAGGING)

#if BUILDFLAG(IS_ANDROID)
// Changes the memory tagging mode for all threads in the current process.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void ChangeMemoryTaggingModeForAllThreadsPerProcess(TagViolationReportingMode);
#endif

// Gets the memory tagging mode for the calling thread.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
TagViolationReportingMode GetMemoryTaggingModeForCurrentThread();

// Called by the partition allocator after initial startup, this detects MTE
// support in the current CPU and replaces the active tagging intrinsics with
// MTE versions if needed.
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void InitializeMTESupportIfNeeded();

// These global function pointers hold the implementations of the tagging
// intrinsics (TagMemoryRangeRandomly, TagMemoryRangeIncrement, RemaskPtr).
// They are designed to be callable without taking a branch. They are initially
// set to no-op functions in tagging.cc, but can be replaced with MTE-capable
// ones through InitializeMTEIfNeeded(). This is conceptually similar to an
// ifunc (but less secure) - we do it this way for now because the CrazyLinker
// (needed for supporting old Android versions) doesn't support them. Initial
// solution should be good enough for fuzzing/debug, ideally needs fixing for
// async deployment on end-user devices.
// TODO(bartekn): void* -> uintptr_t
using RemaskPtrInternalFn = void*(void* ptr);
using TagMemoryRangeIncrementInternalFn = void*(void* ptr, size_t size);

using TagMemoryRangeRandomlyInternalFn = void*(void* ptr,
                                               size_t size,
                                               uint64_t mask);
extern PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    TagMemoryRangeRandomlyInternalFn* global_tag_memory_range_randomly_fn;
extern PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    TagMemoryRangeIncrementInternalFn* global_tag_memory_range_increment_fn;
extern PA_COMPONENT_EXPORT(PARTITION_ALLOC)
    RemaskPtrInternalFn* global_remask_void_ptr_fn;

// Increments the tag of the memory range ptr. Useful for provable revocations
// (e.g. free). Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
// TODO(bartekn): Remove the T* variant.
template <typename T>
PA_ALWAYS_INLINE T* TagMemoryRangeIncrement(T* ptr, size_t size) {
#if defined(PA_HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(global_tag_memory_range_increment_fn(ptr, size));
#else
  return ptr;
#endif
}
PA_ALWAYS_INLINE uintptr_t TagMemoryRangeIncrement(uintptr_t ptr, size_t size) {
  return reinterpret_cast<uintptr_t>(
      TagMemoryRangeIncrement(reinterpret_cast<void*>(ptr), size));
}

// Randomly changes the tag of the ptr memory range. Useful for initial random
// initialization. Returns the pointer with the new tag. Ensures that the entire
// range is set to the same tag.
// TODO(bartekn): Remove the T* variant.
template <typename T>
PA_ALWAYS_INLINE T* TagMemoryRangeRandomly(T* ptr,
                                           size_t size,
                                           uint64_t mask = 0u) {
#if defined(PA_HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(
      global_tag_memory_range_randomly_fn(ptr, size, mask));
#else
  return ptr;
#endif
}
PA_ALWAYS_INLINE uintptr_t TagMemoryRangeRandomly(uintptr_t ptr,
                                                  size_t size,
                                                  uint64_t mask = 0u) {
  return reinterpret_cast<uintptr_t>(
      TagMemoryRangeRandomly(reinterpret_cast<void*>(ptr), size, mask));
}

// Gets a version of ptr that's safe to dereference.
// TODO(bartekn): Remove the T* variant.
template <typename T>
PA_ALWAYS_INLINE T* RemaskPtr(T* ptr) {
#if defined(PA_HAS_MEMORY_TAGGING)
  return reinterpret_cast<T*>(global_remask_void_ptr_fn(ptr));
#else
  return ptr;
#endif
}
// Gets a version of address that's safe to dereference, if cast to a pointer.
PA_ALWAYS_INLINE uintptr_t RemaskPtr(uintptr_t address) {
  return reinterpret_cast<uintptr_t>(
      RemaskPtr(reinterpret_cast<void*>(address)));
}

// Strips the tag bits off address.
PA_ALWAYS_INLINE uintptr_t UnmaskPtr(uintptr_t address) {
#if defined(PA_HAS_MEMORY_TAGGING)
  return address & kMemTagUnmask;
#else
  return address;
#endif
}
// Strips the tag bits off ptr.
// TODO(bartekn): Remove the T* variant.
template <typename T>
PA_ALWAYS_INLINE T* UnmaskPtr(T* ptr) {
  return reinterpret_cast<T*>(UnmaskPtr(reinterpret_cast<uintptr_t>(ptr)));
}

}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_TAGGING_H_
