// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace base {
namespace features {

extern const BASE_EXPORT Feature kPartitionAllocUnretainedDanglingPtr;
enum class UnretainedDanglingPtrMode {
  kCrash,
  kDumpWithoutCrashing,
};
extern const BASE_EXPORT base::FeatureParam<UnretainedDanglingPtrMode>
    kUnretainedDanglingPtrModeParam;

// See /docs/dangling_ptr.md
//
// Usage:
// --enable-features=PartitionAllocDanglingPtr:mode/crash
// --enable-features=PartitionAllocDanglingPtr:mode/log_signature
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocDanglingPtr);
enum class DanglingPtrMode {
  // Crash immediately after detecting a dangling raw_ptr.
  kCrash,  // (default)

  // Log the signature of every occurrences without crashing. It is used by
  // bots.
  // Format "[DanglingSignature]\t<1>\t<2>"
  // 1. The function who freed the memory while it was still referenced.
  // 2. The function who released the raw_ptr reference.
  kLogSignature,

  // Note: This will be extended with a single shot DumpWithoutCrashing.
};
extern const BASE_EXPORT base::FeatureParam<DanglingPtrMode>
    kDanglingPtrModeParam;

#if defined(PA_ALLOW_PCSCAN)
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScan);
#endif  // defined(PA_ALLOW_PCSCAN)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanBrowserOnly);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanRendererOnly);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocBackupRefPtrControl);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocLargeThreadCacheSize);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

enum class BackupRefPtrEnabledProcesses {
  // BRP enabled only in the browser process.
  kBrowserOnly,
  // BRP enabled only in the browser and renderer processes.
  kBrowserAndRenderer,
  // BRP enabled in all processes, except renderer.
  kNonRenderer,
  // BRP enabled in all processes.
  kAllProcesses,
};

enum class BackupRefPtrMode {
  // BRP is disabled across all partitions. Equivalent to the Finch flag being
  // disabled.
  kDisabled,

  // BRP is enabled in the main partition, as well as certain Renderer-only
  // partitions (if enabled in Renderer at all).
  // This entails splitting the main partition.
  kEnabled,

  // Same as kEnabled but without zapping quarantined objects.
  kEnabledWithoutZapping,

  // BRP is disabled, but the main partition is split out, as if BRP was enabled
  // in the "previous slot" mode.
  kDisabledButSplitPartitions2Way,

  // BRP is disabled, but the main partition *and* aligned partition are split
  // out, as if BRP was enabled in the "before allocation" mode.
  kDisabledButSplitPartitions3Way,

  //  BRP is disabled, but add dummy ref count to each allocation. This will
  // increase allocation size but not change any of the logic. If an issue
  // reproduce in this mode, it means the increase in size is causing it.
  kDisabledButAddDummyRefCount,
};

enum class AlternateBucketDistributionMode : uint8_t {
  kDefault,
  kCoarser,
  kDenser,
};

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocBackupRefPtr);
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrEnabledProcesses>
    kBackupRefPtrEnabledProcessesParam;
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrMode>
    kBackupRefPtrModeParam;
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableDereferenceCheckParam;
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableExtractionCheckParam;
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableInstantiationCheckParam;
extern const BASE_EXPORT base::FeatureParam<AlternateBucketDistributionMode>
    kPartitionAllocAlternateBucketDistributionParam;

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanMUAwareScheduler);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanStackScanning);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocDCScan);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanImmediateFreeing);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPCScanEagerClearing);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocSortActiveSlotSpans);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocUseAlternateDistribution);
#if BUILDFLAG(IS_WIN)
BASE_EXPORT BASE_DECLARE_FEATURE(kPageAllocatorRetryOnCommitFailure);
#endif

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
