// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_

#include <cstddef>

#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/base_export.h"

namespace partition_alloc::internal {

// The crash is generated in a PA_NOINLINE function so that we can classify the
// crash as an OOM solely by analyzing the stack trace. It is tagged as
// PA_NOT_TAIL_CALLED to ensure that its parent function stays on the stack.
[[noreturn]] BASE_EXPORT void PA_NOT_TAIL_CALLED OnNoMemory(size_t size);

// OOM_CRASH(size) - Specialization of IMMEDIATE_CRASH which will raise a custom
// exception on Windows to signal this is OOM and not a normal assert.
// OOM_CRASH(size) is called by users of PageAllocator (including
// PartitionAlloc) to signify an allocation failure from the platform.
#define OOM_CRASH(size)                                     \
  do {                                                      \
    /* Raising an exception might allocate, allow that.  */ \
    ::partition_alloc::ScopedAllowAllocations guard{};      \
    ::partition_alloc::internal::OnNoMemory(size);          \
  } while (0)

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_OOM_H_
