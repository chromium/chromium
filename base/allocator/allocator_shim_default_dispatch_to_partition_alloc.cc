// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/allocator_shim.h"
#include "base/allocator/allocator_shim_internals.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/bits.h"
#include "base/no_destructor.h"

namespace {

base::ThreadSafePartitionRoot& Allocator() {
  static base::NoDestructor<base::ThreadSafePartitionRoot> allocator{
      false /* enforce_alignment */};
  return *allocator;
}

using base::allocator::AllocatorDispatch;

void* PartitionMalloc(const AllocatorDispatch*, size_t size, void* context) {
  return Allocator().AllocFlagsNoHooks(0, size);
}

void* PartitionCalloc(const AllocatorDispatch*,
                      size_t n,
                      size_t size,
                      void* context) {
  return Allocator().AllocFlagsNoHooks(base::PartitionAllocZeroFill, n * size);
}

void* PartitionMemalign(const AllocatorDispatch*,
                        size_t alignment,
                        size_t size,
                        void* context) {
  static base::NoDestructor<base::ThreadSafePartitionRoot> aligned_allocator{
      true /* enforce_alignment */};
  return aligned_allocator->AlignedAllocFlags(base::PartitionAllocNoHooks,
                                              alignment, size);
}

void* PartitionRealloc(const AllocatorDispatch*,
                       void* address,
                       size_t size,
                       void* context) {
  return Allocator().ReallocFlags(base::PartitionAllocNoHooks, address, size,
                                  "");
}

void PartitionFree(const AllocatorDispatch*, void* address, void* context) {
  base::ThreadSafePartitionRoot::FreeNoHooks(address);
}

size_t PartitionGetSizeEstimate(const AllocatorDispatch*,
                                void* address,
                                void* context) {
  // TODO(lizeb): Returns incorrect values for aligned allocations.
  return base::ThreadSafePartitionRoot::GetAllocatedSize(address);
}

}  // namespace

constexpr AllocatorDispatch AllocatorDispatch::default_dispatch = {
    &PartitionMalloc,          /* alloc_function */
    &PartitionCalloc,          /* alloc_zero_initialized_function */
    &PartitionMemalign,        /* alloc_aligned_function */
    &PartitionRealloc,         /* realloc_function */
    &PartitionFree,            /* free_function */
    &PartitionGetSizeEstimate, /* get_size_estimate_function */
    nullptr,                   /* batch_malloc_function */
    nullptr,                   /* batch_free_function */
    nullptr,                   /* free_definite_size_function */
    nullptr,                   /* aligned_malloc_function */
    nullptr,                   /* aligned_realloc_function */
    nullptr,                   /* aligned_free_function */
    nullptr,                   /* next */
};

// Intercept diagnostics symbols as well, even though they are not part of the
// unified shim layer.
//
// TODO(lizeb): Implement the ones that doable.

extern "C" {

SHIM_ALWAYS_EXPORT void malloc_stats(void) __THROW {}

SHIM_ALWAYS_EXPORT int mallopt(int cmd, int value) __THROW {
  return 0;
}

#ifdef HAVE_STRUCT_MALLINFO
SHIM_ALWAYS_EXPORT struct mallinfo mallinfo(void) __THROW {
  return {};
}
#endif

}  // extern "C"
