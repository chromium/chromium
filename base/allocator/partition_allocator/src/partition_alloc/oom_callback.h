// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_OOM_CALLBACK_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_OOM_CALLBACK_H_

#include "partition_alloc/partition_alloc_base/component_export.h"

namespace partition_alloc {

using PartitionAllocOomCallback = void (*)();

// Registers a callback to be invoked during an OOM_CRASH(). OOM_CRASH is
// invoked by users of PageAllocator (including PartitionAlloc) to signify an
// allocation failure from the platform.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void SetPartitionAllocOomCallback(PartitionAllocOomCallback callback);

namespace internal {
PA_COMPONENT_EXPORT(PARTITION_ALLOC) void RunPartitionAllocOomCallback();
}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_OOM_CALLBACK_H_
