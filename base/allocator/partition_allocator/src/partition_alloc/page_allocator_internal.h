// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_
#define PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_

#include <cstddef>
#include <cstdint>

#include "partition_alloc/page_allocator.h"

namespace partition_alloc::internal {

uintptr_t SystemAllocPages(uintptr_t hint,
                           size_t length,
                           PageAccessibilityConfiguration accessibility,
                           PageTag page_tag,
                           int file_descriptor_for_shared_alloc = -1);

// Returns the size of the OS-based zero segment which is a red zone
// region that cannot be reserved starting at address 0x0. The segments
// permissions may be a mix of read-only and no access.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
size_t GetZeroSegmentSizeFromOS();

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PAGE_ALLOCATOR_INTERNAL_H_
