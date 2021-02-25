// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_features.h"

#include "base/feature_list.h"

namespace base {
namespace features {

#if PA_ALLOW_PCSCAN
// If enabled, PCScan is turned on by default for all partitions that don't
// disable it explicitly.
const Feature kPartitionAllocPCScan{"PartitionAllocPCScan",
                                    FEATURE_DISABLED_BY_DEFAULT};
#endif  // PA_ALLOW_PCSCAN

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// If enabled, PCScan is turned on only for the browser's malloc partition.
const Feature kPartitionAllocPCScanBrowserOnly{
    "PartitionAllocPCScanBrowserOnly", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the thread cache will be periodically purged.
const Feature kPartitionAllocThreadCachePeriodicPurge{
    "PartitionAllocThreadCachePeriodicPurge", FEATURE_DISABLED_BY_DEFAULT};

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

const Feature kPartitionAllocLazyCommit{"PartitionAllocLazyCommit",
                                        FEATURE_ENABLED_BY_DEFAULT};

}  // namespace features
}  // namespace base
