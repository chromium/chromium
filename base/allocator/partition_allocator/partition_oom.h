// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Holds functions for generating OOM errors from PartitionAlloc. This is
// distinct from oom.h in that it is meant only for use in PartitionAlloc.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_

#include <stddef.h>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "build/build_config.h"

namespace partition_alloc {

using OomFunction = void (*)(size_t);

namespace internal {

// g_oom_handling_function is invoked when PartitionAlloc hits OutOfMemory.
extern OomFunction g_oom_handling_function;

[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void PartitionExcessiveAllocationSize(size_t size);

#if !defined(ARCH_CPU_64_BITS)
[[noreturn]] PA_NOINLINE void PartitionOutOfMemoryWithLotsOfUncommitedPages(
    size_t size);
[[noreturn]] PA_NOINLINE void PartitionOutOfMemoryWithLargeVirtualSize(
    size_t virtual_size);
#endif

}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_OOM_H_
