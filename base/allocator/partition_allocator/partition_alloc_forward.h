// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_

#include "base/base_export.h"

namespace base {
namespace internal {

template <bool thread_safe>
struct PartitionPage;

BASE_EXPORT size_t PartitionAllocGetSlotOffset(void* ptr);

constexpr bool ThreadSafe = true;
constexpr bool NotThreadSafe = false;

}  // namespace internal

template <bool thread_safe>
struct PartitionRoot;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FORWARD_H_
