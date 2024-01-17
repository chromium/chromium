// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_INTERNAL_ALLOCATOR_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_INTERNAL_ALLOCATOR_H_

#include <new>
#include <type_traits>

#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_root.h"

// Internal Allocator can be used to get heap allocations required to
// implement Partition Allocator's feature.
// As Internal Allocator being Partition Allocator with minimal configuration,
// it is not allowed to use this allocator for PA's core implementation to avoid
// reentrancy issues. Also don't use this when satisfying the very first PA-E
// allocation of the process.

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
PartitionRoot& InternalAllocatorRoot();

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_INTERNAL_ALLOCATOR_H_
