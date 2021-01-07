// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/dcheck_is_on.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct SlotSpanMetadata;

constexpr bool ThreadSafe = true;
constexpr bool NotThreadSafe = false;

#if DCHECK_IS_ON()
BASE_EXPORT void DCheckGetSlotOffsetIsZero(void*);
#else
ALWAYS_INLINE void DCheckGetSlotOffsetIsZero(void*) {}
#endif
}  // namespace internal

template <bool thread_safe>
struct PartitionRoot;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
