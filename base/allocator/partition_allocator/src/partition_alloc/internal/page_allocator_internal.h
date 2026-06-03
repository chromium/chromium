// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PAGE_ALLOCATOR_INTERNAL_H_
#define PARTITION_ALLOC_INTERNAL_PAGE_ALLOCATOR_INTERNAL_H_

#include <array>
#include <cstddef>
#include <cstdint>

#include "partition_alloc/page_allocator.h"

namespace partition_alloc::internal {

uintptr_t SystemAllocPages(uintptr_t hint,
                           size_t length,
                           PageAccessibilityConfiguration accessibility,
                           PageTag page_tag,
                           int file_descriptor_for_shared_alloc = -1);

// Represents a generic range of memory defined by a starting address and size.
struct AddressRange {
  uintptr_t address;
  size_t size;
};

struct WellKnownReadOnlyRegions {
  // Represents a safe, well-known region of memory that is already occupied,
  // reserved, or otherwise handled by the OS, and is guaranteed to be
  // read-only or inaccessible.
  using WellKnownReadOnlyRegion = AddressRange;

  // Maximum number of well known regions.
  static inline constexpr size_t kMaxRegions = 8;

  std::array<WellKnownReadOnlyRegion, kMaxRegions> regions;
  size_t count = 0;
};

// Returns the list of well-known read-only regions for the current platform.
// The regions are sorted by address and non-overlapping.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
WellKnownReadOnlyRegions GetWellKnownReadOnlyRegions();

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_INTERNAL_PAGE_ALLOCATOR_INTERNAL_H_
