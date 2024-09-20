// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_root.h"

namespace base {
namespace features {

namespace internal {

enum class PAFeatureEnabledProcesses {
  // Enabled only in the browser process.
  kBrowserOnly,
  // Enabled only in the browser and renderer processes.
  kBrowserAndRenderer,
  // Enabled in all processes, except renderer.
  kNonRenderer,
  // Enabled only in renderer processes.
  kRendererOnly,
  // Enabled in all child processes, except zygote.
  kAllChildProcesses,
  // Enabled in all processes.
  kAllProcesses,
};

}  // namespace internal

extern const BASE_EXPORT Feature kPartitionAllocUnretainedDanglingPtr;
enum class UnretainedDanglingPtrMode {
  kCrash,
  kDumpWithoutCrashing,
};
extern const BASE_EXPORT base::FeatureParam<UnretainedDanglingPtrMode>
    kUnretainedDanglingPtrModeParam;

// See /docs/dangling_ptr.md
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocDanglingPtr);
enum class DanglingPtrMode {
  // Crash immediately after detecting a dangling raw_ptr.
  kCrash,  // (default)

  // Log the signature of every occurrences without crashing. It is used by
  // bots.
  // Format "[DanglingSignature]\t<1>\t<2>\t<3>\t<4>"
  // 1. The function which freed the memory while it was still referenced.
  // 2. The task in which the memory was freed.
  // 3. The function which released the raw_ptr reference.
  // 4. The task in which the raw_ptr was released.
  kLogOnly,

  // Note: This will be extended with a single shot DumpWithoutCrashing.
};
extern const BASE_EXPORT base::FeatureParam<DanglingPtrMode>
    kDanglingPtrModeParam;
enum class DanglingPtrType {
  // Act on any dangling raw_ptr released after being freed.
  kAll,  // (default)

  // Detect when freeing memory and releasing the dangling raw_ptr happens in
  // a different task. Those are more likely to cause use after free.
  kCrossTask,

  // Note: This will be extended with LongLived
};
extern const BASE_EXPORT base::FeatureParam<DanglingPtrType>
    kDanglingPtrTypeParam;

using PartitionAllocWithAdvancedChecksEnabledProcesses =
    internal::PAFeatureEnabledProcesses;

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocLargeThreadCacheSize);
BASE_EXPORT int GetPartitionAllocLargeThreadCacheSizeValue();
BASE_EXPORT int GetPartitionAllocLargeThreadCacheSizeValueForLowRAMAndroid();

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocWithAdvancedChecks);
extern const BASE_EXPORT
    base::FeatureParam<PartitionAllocWithAdvancedChecksEnabledProcesses>
        kPartitionAllocWithAdvancedChecksEnabledProcessesParam;
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocSchedulerLoopQuarantine);
// Scheduler Loop Quarantine's per-thread capacity in bytes.
extern const BASE_EXPORT base::FeatureParam<int>
    kPartitionAllocSchedulerLoopQuarantineBranchCapacity;

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocZappingByFreeFlags);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

using BackupRefPtrEnabledProcesses = internal::PAFeatureEnabledProcesses;

enum class BackupRefPtrMode {
  // BRP is disabled across all partitions. Equivalent to the Finch flag being
  // disabled.
  kDisabled,

  // BRP is enabled in the main partition, as well as certain Renderer-only
  // partitions (if enabled in Renderer at all).
  kEnabled,
};

enum class MemtagMode {
  // memtagMode will be SYNC.
  kSync,
  // memtagMode will be ASYNC.
  kAsync,
};

enum class RetagMode {
  // Allocations are retagged by incrementing the current tag.
  kIncrement,

  // Allocations are retagged with a random tag.
  kRandom,
};

using MemoryTaggingEnabledProcesses = internal::PAFeatureEnabledProcesses;

enum class BucketDistributionMode : uint8_t {
  kDefault,
  kDenser,
};

// Parameter for 'kPartitionAllocMakeFreeNoOpOnShutdown' feature which
// controls when free() becomes a no-op during Shutdown()
enum class WhenFreeBecomesNoOp {
  kBeforePreShutdown,
  kBeforeHaltingStartupTracingController,
  kBeforeShutDownThreads,
  kInShutDownThreads,
  kAfterShutDownThreads,
};

// Inserts a no-op on 'free()' allocator shim at the front of the
// dispatch chain if called from the appropriate callsite.
BASE_EXPORT void MakeFreeNoOp(WhenFreeBecomesNoOp callsite);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocMakeFreeNoOpOnShutdown);
extern const BASE_EXPORT base::FeatureParam<WhenFreeBecomesNoOp>
    kPartitionAllocMakeFreeNoOpOnShutdownParam;

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocBackupRefPtr);
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrEnabledProcesses>
    kBackupRefPtrEnabledProcessesParam;
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrMode>
    kBackupRefPtrModeParam;
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocMemoryTagging);
extern const BASE_EXPORT base::FeatureParam<MemtagMode> kMemtagModeParam;
extern const BASE_EXPORT base::FeatureParam<RetagMode> kRetagModeParam;
extern const BASE_EXPORT base::FeatureParam<MemoryTaggingEnabledProcesses>
    kMemoryTaggingEnabledProcessesParam;
// Kill switch for memory tagging. Skips any code related to memory tagging when
// enabled.
BASE_EXPORT BASE_DECLARE_FEATURE(kKillPartitionAllocMemoryTagging);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPermissiveMte);
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableDereferenceCheckParam;
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableExtractionCheckParam;
extern const BASE_EXPORT base::FeatureParam<bool>
    kBackupRefPtrAsanEnableInstantiationCheckParam;
extern const BASE_EXPORT base::FeatureParam<BucketDistributionMode>
    kPartitionAllocBucketDistributionParam;

BASE_EXPORT BASE_DECLARE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocUseDenserDistribution);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocMemoryReclaimer);
extern const BASE_EXPORT base::FeatureParam<TimeDelta>
    kPartitionAllocMemoryReclaimerInterval;
BASE_EXPORT BASE_DECLARE_FEATURE(
    kPartitionAllocStraightenLargerSlotSpanFreeLists);
extern const BASE_EXPORT
    base::FeatureParam<partition_alloc::StraightenLargerSlotSpanFreeListsMode>
        kPartitionAllocStraightenLargerSlotSpanFreeListsMode;
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocSortSmallerSlotSpanFreeLists);
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocSortActiveSlotSpans);

#if BUILDFLAG(IS_WIN)
BASE_EXPORT BASE_DECLARE_FEATURE(kPageAllocatorRetryOnCommitFailure);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
extern const base::FeatureParam<bool>
    kPartialLowEndModeExcludePartitionAllocSupport;
#endif

BASE_EXPORT BASE_DECLARE_FEATURE(kEnableConfigurableThreadCacheMultiplier);
BASE_EXPORT double GetThreadCacheMultiplier();
BASE_EXPORT double GetThreadCacheMultiplierForAndroid();

BASE_EXPORT BASE_DECLARE_FEATURE(kEnableConfigurableThreadCachePurgeInterval);
extern const partition_alloc::internal::base::TimeDelta
GetThreadCacheMinPurgeInterval();
extern const partition_alloc::internal::base::TimeDelta
GetThreadCacheMaxPurgeInterval();
extern const partition_alloc::internal::base::TimeDelta
GetThreadCacheDefaultPurgeInterval();

BASE_EXPORT BASE_DECLARE_FEATURE(
    kEnableConfigurableThreadCacheMinCachedMemoryForPurging);
BASE_EXPORT int GetThreadCacheMinCachedMemoryForPurgingBytes();

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocDisableBRPInBufferPartition);

// This feature is additionally gated behind a buildflag because
// pool offset freelists cannot be represented when PartitionAlloc uses
// 32-bit pointers.
#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
BASE_EXPORT BASE_DECLARE_FEATURE(kUsePoolOffsetFreelists);
#endif

// When set, partitions use a larger ring buffer and free memory less
// aggressively when in the foreground.
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocAdjustSizeWhenInForeground);

// When enabled, uses a more nuanced heuristic to determine if slot
// spans can be treated as "single-slot."
//
// See also: https://crbug.com/333443437
BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocUseSmallSingleSlotSpans);

#if PA_CONFIG(ENABLE_SHADOW_METADATA)
using ShadowMetadataEnabledProcesses = internal::PAFeatureEnabledProcesses;

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocShadowMetadata);
extern const BASE_EXPORT base::FeatureParam<ShadowMetadataEnabledProcesses>
    kShadowMetadataEnabledProcessesParam;
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
