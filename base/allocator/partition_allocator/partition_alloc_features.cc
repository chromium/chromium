// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_features.h"

#include "base/feature_list.h"

namespace base {
namespace features {

#if defined(PA_ALLOW_PCSCAN)
// If enabled, PCScan is turned on by default for all partitions that don't
// disable it explicitly.
const Feature kPartitionAllocPCScan{"PartitionAllocPCScan",
                                    FEATURE_DISABLED_BY_DEFAULT};
#endif  // defined(PA_ALLOW_PCSCAN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// If enabled, PCScan is turned on only for the browser's malloc partition.
const Feature kPartitionAllocPCScanBrowserOnly{
    "PartitionAllocPCScanBrowserOnly", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, this instance belongs to the Control group of the BackupRefPtr
// binary experiment.
const Feature kPartitionAllocBackupRefPtrControl{
    "PartitionAllocBackupRefPtrControl", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, the thread cache will be periodically purged.
const Feature kPartitionAllocThreadCachePeriodicPurge{
    "PartitionAllocThreadCachePeriodicPurge", FEATURE_ENABLED_BY_DEFAULT};

// Use a larger maximum thread cache cacheable bucket size.
const Feature kPartitionAllocLargeThreadCacheSize{
    "PartitionAllocLargeThreadCacheSize", FEATURE_DISABLED_BY_DEFAULT};

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

const Feature kPartitionAllocLazyCommit{"PartitionAllocLazyCommit",
                                        FEATURE_ENABLED_BY_DEFAULT};

// If enabled, switches PCScan scheduling to a mutator-aware scheduler. Does not
// affect whether PCScan is enabled itself.
const Feature kPartitionAllocPCScanMUAwareScheduler{
    "PartitionAllocPCScanMUAwareScheduler", FEATURE_ENABLED_BY_DEFAULT};

// If enabled, PCScan frees unconditionally all quarantined objects.
// This is a performance testing feature.
const Feature kPartitionAllocPCScanImmediateFreeing{
    "PartitionAllocPCScanImmediateFreeing", FEATURE_DISABLED_BY_DEFAULT};

// In addition to heap, scan also the stack of the current mutator.
const Feature kPartitionAllocPCScanStackScanning {
  "PartitionAllocPCScanStackScanning",
#if defined(PA_PCSCAN_STACK_SUPPORTED)
      FEATURE_ENABLED_BY_DEFAULT
#else
      FEATURE_DISABLED_BY_DEFAULT
#endif  // defined(PA_PCSCAN_STACK_SUPPORTED)
};

const Feature kPartitionAllocDCScan{"PartitionAllocDCScan",
                                    FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace base
