// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_features.h"

#include "base/feature_list.h"

namespace base {
namespace features {

#if defined(PA_HAS_64_BITS_POINTERS)
// If enabled, PartitionAllocator reserves an address space(named, giga cage)
// initially and uses a part of the address space for each allocation.
const Feature kPartitionAllocGigaCage{"PartitionAllocGigaCage",
                                      FEATURE_ENABLED_BY_DEFAULT};
#else
// If enabled, PartitionAllocator remembers allocated address space.
const Feature kPartitionAllocGigaCage{"PartitionAllocGigaCage32bit",
                                      FEATURE_ENABLED_BY_DEFAULT};
#endif

// If enabled, PCScan is turned on by default for all partitions that don't
// disable it explicitly.
const Feature kPartitionAllocPCScan{"PartitionAllocPCScan",
                                    FEATURE_DISABLED_BY_DEFAULT};

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// If enabled, PCScan is turned on only for the browser's malloc partition.
const Feature kPartitionAllocPCScanBrowserOnly{
    "PartitionAllocPCScanBrowserOnly", FEATURE_DISABLED_BY_DEFAULT};
#endif

}  // namespace features
}  // namespace base
