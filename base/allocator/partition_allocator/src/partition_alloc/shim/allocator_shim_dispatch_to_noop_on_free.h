// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#ifndef PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DISPATCH_TO_NOOP_ON_FREE_H_
#define PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DISPATCH_TO_NOOP_ON_FREE_H_

#include "partition_alloc/partition_alloc_base/component_export.h"

namespace allocator_shim {
// Places an allocator shim layer at the front of the chain during shutdown.
// This new layer replaces free() with a no-op implementation
// in order to prevent shutdown hangs.
PA_COMPONENT_EXPORT(ALLOCATOR_SHIM)
void InsertNoOpOnFreeAllocatorShimOnShutDown();
}  // namespace allocator_shim

#endif  // PARTITION_ALLOC_SHIM_ALLOCATOR_SHIM_DISPATCH_TO_NOOP_ON_FREE_H_
