// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_features.h"

#include "base/allocator/miracle_parameter.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/shim/allocator_shim_dispatch_to_noop_on_free.h"
#include "partition_alloc/thread_cache.h"

namespace base {
namespace features {

BASE_FEATURE(kPartitionAllocUnretainedDanglingPtr,
             "PartitionAllocUnretainedDanglingPtr",
             FEATURE_ENABLED_BY_DEFAULT);

constexpr FeatureParam<UnretainedDanglingPtrMode>::Option
    kUnretainedDanglingPtrModeOption[] = {
        {UnretainedDanglingPtrMode::kCrash, "crash"},
        {UnretainedDanglingPtrMode::kDumpWithoutCrashing,
         "dump_without_crashing"},
};
const base::FeatureParam<UnretainedDanglingPtrMode>
    kUnretainedDanglingPtrModeParam = {
        &kPartitionAllocUnretainedDanglingPtr,
        "mode",
        UnretainedDanglingPtrMode::kCrash,
        &kUnretainedDanglingPtrModeOption,
};

BASE_FEATURE(kPartitionAllocDanglingPtr,
             "PartitionAllocDanglingPtr",
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_FEATURE_FLAG)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr FeatureParam<DanglingPtrMode>::Option kDanglingPtrModeOption[] = {
    {DanglingPtrMode::kCrash, "crash"},
    {DanglingPtrMode::kLogOnly, "log_only"},
};
const base::FeatureParam<DanglingPtrMode> kDanglingPtrModeParam{
    &kPartitionAllocDanglingPtr,
    "mode",
    DanglingPtrMode::kCrash,
    &kDanglingPtrModeOption,
};
constexpr FeatureParam<DanglingPtrType>::Option kDanglingPtrTypeOption[] = {
    {DanglingPtrType::kAll, "all"},
    {DanglingPtrType::kCrossTask, "cross_task"},
};
const base::FeatureParam<DanglingPtrType> kDanglingPtrTypeParam{
    &kPartitionAllocDanglingPtr,
    "type",
    DanglingPtrType::kAll,
    &kDanglingPtrTypeOption,
};

#if PA_BUILDFLAG(USE_STARSCAN)
// If enabled, PCScan is turned on by default for all partitions that don't
// disable it explicitly.
BASE_FEATURE(kPartitionAllocPCScan,
             "PartitionAllocPCScan",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // PA_BUILDFLAG(USE_STARSCAN)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// If enabled, PCScan is turned on only for the browser's malloc partition.
BASE_FEATURE(kPartitionAllocPCScanBrowserOnly,
             "PartitionAllocPCScanBrowserOnly",
             FEATURE_DISABLED_BY_DEFAULT);

// If enabled, PCScan is turned on only for the renderer's malloc partition.
BASE_FEATURE(kPartitionAllocPCScanRendererOnly,
             "PartitionAllocPCScanRendererOnly",
             FEATURE_DISABLED_BY_DEFAULT);

// Use a larger maximum thread cache cacheable bucket size.
BASE_FEATURE(kPartitionAllocLargeThreadCacheSize,
             "PartitionAllocLargeThreadCacheSize",
             FEATURE_ENABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_INT(GetPartitionAllocLargeThreadCacheSizeValue,
                          kPartitionAllocLargeThreadCacheSize,
                          "PartitionAllocLargeThreadCacheSizeValue",
                          ::partition_alloc::kThreadCacheLargeSizeThreshold)

MIRACLE_PARAMETER_FOR_INT(
    GetPartitionAllocLargeThreadCacheSizeValueForLowRAMAndroid,
    kPartitionAllocLargeThreadCacheSize,
    "PartitionAllocLargeThreadCacheSizeValueForLowRAMAndroid",
    ::partition_alloc::kThreadCacheDefaultSizeThreshold)

BASE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing,
             "PartitionAllocLargeEmptySlotSpanRing",
#if BUILDFLAG(IS_MAC)
             FEATURE_ENABLED_BY_DEFAULT);
#else
             FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPartitionAllocSchedulerLoopQuarantine,
             "PartitionAllocSchedulerLoopQuarantine",
             FEATURE_DISABLED_BY_DEFAULT);
// Scheduler Loop Quarantine's per-branch capacity in bytes.
const base::FeatureParam<int>
    kPartitionAllocSchedulerLoopQuarantineBranchCapacity{
        &kPartitionAllocSchedulerLoopQuarantine,
        "PartitionAllocSchedulerLoopQuarantineBranchCapacity", 0};

BASE_FEATURE(kPartitionAllocZappingByFreeFlags,
             "PartitionAllocZappingByFreeFlags",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

BASE_FEATURE(kPartitionAllocBackupRefPtr,
             "PartitionAllocBackupRefPtr",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) ||     \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) ||                  \
    PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr FeatureParam<BackupRefPtrEnabledProcesses>::Option
    kBackupRefPtrEnabledProcessesOptions[] = {
        {BackupRefPtrEnabledProcesses::kBrowserOnly, "browser-only"},
        {BackupRefPtrEnabledProcesses::kBrowserAndRenderer,
         "browser-and-renderer"},
        {BackupRefPtrEnabledProcesses::kNonRenderer, "non-renderer"},
        {BackupRefPtrEnabledProcesses::kAllProcesses, "all-processes"}};

const base::FeatureParam<BackupRefPtrEnabledProcesses>
    kBackupRefPtrEnabledProcessesParam{
        &kPartitionAllocBackupRefPtr, "enabled-processes",
        BackupRefPtrEnabledProcesses::kNonRenderer,
        &kBackupRefPtrEnabledProcessesOptions};

constexpr FeatureParam<BackupRefPtrMode>::Option kBackupRefPtrModeOptions[] = {
    {BackupRefPtrMode::kDisabled, "disabled"},
    {BackupRefPtrMode::kEnabled, "enabled"},
};

const base::FeatureParam<BackupRefPtrMode> kBackupRefPtrModeParam{
    &kPartitionAllocBackupRefPtr, "brp-mode", BackupRefPtrMode::kEnabled,
    &kBackupRefPtrModeOptions};

BASE_FEATURE(kPartitionAllocMemoryTagging,
             "PartitionAllocMemoryTagging",
#if PA_BUILDFLAG(USE_FULL_MTE)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr FeatureParam<MemtagMode>::Option kMemtagModeOptions[] = {
    {MemtagMode::kSync, "sync"},
    {MemtagMode::kAsync, "async"}};

const base::FeatureParam<MemtagMode> kMemtagModeParam{
    &kPartitionAllocMemoryTagging, "memtag-mode",
#if PA_BUILDFLAG(USE_FULL_MTE)
    MemtagMode::kSync,
#else
    MemtagMode::kAsync,
#endif
    &kMemtagModeOptions};

constexpr FeatureParam<MemoryTaggingEnabledProcesses>::Option
    kMemoryTaggingEnabledProcessesOptions[] = {
        {MemoryTaggingEnabledProcesses::kBrowserOnly, "browser-only"},
        {MemoryTaggingEnabledProcesses::kNonRenderer, "non-renderer"},
        {MemoryTaggingEnabledProcesses::kAllProcesses, "all-processes"}};

const base::FeatureParam<MemoryTaggingEnabledProcesses>
    kMemoryTaggingEnabledProcessesParam{
        &kPartitionAllocMemoryTagging, "enabled-processes",
#if PA_BUILDFLAG(USE_FULL_MTE)
        MemoryTaggingEnabledProcesses::kAllProcesses,
#else
        MemoryTaggingEnabledProcesses::kBrowserOnly,
#endif
        &kMemoryTaggingEnabledProcessesOptions};

BASE_FEATURE(kKillPartitionAllocMemoryTagging,
             "KillPartitionAllocMemoryTagging",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPermissiveMte);
BASE_FEATURE(kPartitionAllocPermissiveMte,
             "PartitionAllocPermissiveMte",
#if PA_BUILDFLAG(USE_FULL_MTE)
             // We want to actually crash if USE_FULL_MTE is enabled.
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif
);

const base::FeatureParam<bool> kBackupRefPtrAsanEnableDereferenceCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-dereference-check", true};
const base::FeatureParam<bool> kBackupRefPtrAsanEnableExtractionCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-extraction-check",
    false};  // Not much noise at the moment to enable by default.
const base::FeatureParam<bool> kBackupRefPtrAsanEnableInstantiationCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-instantiation-check", true};

// If enabled, switches the bucket distribution to a denser one.
//
// We enable this by default everywhere except for 32-bit Android, since we saw
// regressions there.
BASE_FEATURE(kPartitionAllocUseDenserDistribution,
             "PartitionAllocUseDenserDistribution",
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
);
const base::FeatureParam<BucketDistributionMode>::Option
    kPartitionAllocBucketDistributionOption[] = {
        {BucketDistributionMode::kDefault, "default"},
        {BucketDistributionMode::kDenser, "denser"},
};
const base::FeatureParam<BucketDistributionMode>
    kPartitionAllocBucketDistributionParam {
  &kPartitionAllocUseDenserDistribution, "mode",
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
      BucketDistributionMode::kDefault,
#else
      BucketDistributionMode::kDenser,
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
      &kPartitionAllocBucketDistributionOption
};

BASE_FEATURE(kPartitionAllocMemoryReclaimer,
             "PartitionAllocMemoryReclaimer",
             FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<TimeDelta> kPartitionAllocMemoryReclaimerInterval = {
    &kPartitionAllocMemoryReclaimer, "interval",
    TimeDelta(),  // Defaults to zero.
};

// Configures whether we set a lower limit for renderers that do not have a main
// frame, similar to the limit that is already done for backgrounded renderers.
BASE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers,
             "LowerPAMemoryLimitForNonMainRenderers",
             FEATURE_DISABLED_BY_DEFAULT);

// If enabled, switches PCScan scheduling to a mutator-aware scheduler. Does not
// affect whether PCScan is enabled itself.
BASE_FEATURE(kPartitionAllocPCScanMUAwareScheduler,
             "PartitionAllocPCScanMUAwareScheduler",
             FEATURE_ENABLED_BY_DEFAULT);

// If enabled, PCScan frees unconditionally all quarantined objects.
// This is a performance testing feature.
BASE_FEATURE(kPartitionAllocPCScanImmediateFreeing,
             "PartitionAllocPCScanImmediateFreeing",
             FEATURE_DISABLED_BY_DEFAULT);

// If enabled, PCScan clears eagerly (synchronously) on free().
BASE_FEATURE(kPartitionAllocPCScanEagerClearing,
             "PartitionAllocPCScanEagerClearing",
             FEATURE_DISABLED_BY_DEFAULT);

// In addition to heap, scan also the stack of the current mutator.
BASE_FEATURE(kPartitionAllocPCScanStackScanning,
             "PartitionAllocPCScanStackScanning",
#if PA_BUILDFLAG(STACK_SCAN_SUPPORTED)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif  // PA_BUILDFLAG(STACK_SCAN_SUPPORTED)
);

BASE_FEATURE(kPartitionAllocDCScan,
             "PartitionAllocDCScan",
             FEATURE_DISABLED_BY_DEFAULT);

// Whether to straighten free lists for larger slot spans in PurgeMemory() ->
// ... -> PartitionPurgeSlotSpan().
BASE_FEATURE(kPartitionAllocStraightenLargerSlotSpanFreeLists,
             "PartitionAllocStraightenLargerSlotSpanFreeLists",
             FEATURE_ENABLED_BY_DEFAULT);
const base::FeatureParam<
    partition_alloc::StraightenLargerSlotSpanFreeListsMode>::Option
    kPartitionAllocStraightenLargerSlotSpanFreeListsModeOption[] = {
        {partition_alloc::StraightenLargerSlotSpanFreeListsMode::
             kOnlyWhenUnprovisioning,
         "only-when-unprovisioning"},
        {partition_alloc::StraightenLargerSlotSpanFreeListsMode::kAlways,
         "always"},
};
const base::FeatureParam<partition_alloc::StraightenLargerSlotSpanFreeListsMode>
    kPartitionAllocStraightenLargerSlotSpanFreeListsMode = {
        &kPartitionAllocStraightenLargerSlotSpanFreeLists,
        "mode",
        partition_alloc::StraightenLargerSlotSpanFreeListsMode::
            kOnlyWhenUnprovisioning,
        &kPartitionAllocStraightenLargerSlotSpanFreeListsModeOption,
};

// Whether to sort free lists for smaller slot spans in PurgeMemory().
BASE_FEATURE(kPartitionAllocSortSmallerSlotSpanFreeLists,
             "PartitionAllocSortSmallerSlotSpanFreeLists",
             FEATURE_ENABLED_BY_DEFAULT);

// Whether to sort the active slot spans in PurgeMemory().
BASE_FEATURE(kPartitionAllocSortActiveSlotSpans,
             "PartitionAllocSortActiveSlotSpans",
             FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Whether to retry allocations when commit fails.
BASE_FEATURE(kPageAllocatorRetryOnCommitFailure,
             "PageAllocatorRetryOnCommitFailure",
             FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude PartitionAllocSupport from
// PartialLowModeOnMidRangeDevices. This is used to see how it affects
// renderer performances, e.g. blink_perf.parser benchmark.
// The feature: kPartialLowEndModeOnMidRangeDevices is defined in
// //base/features.cc. Since the following feature param is related to
// PartitionAlloc, define the param here.
const FeatureParam<bool> kPartialLowEndModeExcludePartitionAllocSupport{
    &kPartialLowEndModeOnMidRangeDevices, "exclude-partition-alloc-support",
    false};
#endif

BASE_FEATURE(kEnableConfigurableThreadCacheMultiplier,
             "EnableConfigurableThreadCacheMultiplier",
             base::FEATURE_DISABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_DOUBLE(GetThreadCacheMultiplier,
                             kEnableConfigurableThreadCacheMultiplier,
                             "ThreadCacheMultiplier",
                             2.)

MIRACLE_PARAMETER_FOR_DOUBLE(GetThreadCacheMultiplierForAndroid,
                             kEnableConfigurableThreadCacheMultiplier,
                             "ThreadCacheMultiplierForAndroid",
                             1.)

constexpr partition_alloc::internal::base::TimeDelta ToPartitionAllocTimeDelta(
    base::TimeDelta time_delta) {
  return partition_alloc::internal::base::Microseconds(
      time_delta.InMicroseconds());
}

constexpr base::TimeDelta FromPartitionAllocTimeDelta(
    partition_alloc::internal::base::TimeDelta time_delta) {
  return base::Microseconds(time_delta.InMicroseconds());
}

BASE_FEATURE(kEnableConfigurableThreadCachePurgeInterval,
             "EnableConfigurableThreadCachePurgeInterval",
             base::FEATURE_DISABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_TIME_DELTA(
    GetThreadCacheMinPurgeIntervalValue,
    kEnableConfigurableThreadCachePurgeInterval,
    "ThreadCacheMinPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kMinPurgeInterval))

MIRACLE_PARAMETER_FOR_TIME_DELTA(
    GetThreadCacheMaxPurgeIntervalValue,
    kEnableConfigurableThreadCachePurgeInterval,
    "ThreadCacheMaxPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kMaxPurgeInterval))

MIRACLE_PARAMETER_FOR_TIME_DELTA(
    GetThreadCacheDefaultPurgeIntervalValue,
    kEnableConfigurableThreadCachePurgeInterval,
    "ThreadCacheDefaultPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kDefaultPurgeInterval))

const partition_alloc::internal::base::TimeDelta
GetThreadCacheMinPurgeInterval() {
  return ToPartitionAllocTimeDelta(GetThreadCacheMinPurgeIntervalValue());
}

const partition_alloc::internal::base::TimeDelta
GetThreadCacheMaxPurgeInterval() {
  return ToPartitionAllocTimeDelta(GetThreadCacheMaxPurgeIntervalValue());
}

const partition_alloc::internal::base::TimeDelta
GetThreadCacheDefaultPurgeInterval() {
  return ToPartitionAllocTimeDelta(GetThreadCacheDefaultPurgeIntervalValue());
}

BASE_FEATURE(kEnableConfigurableThreadCacheMinCachedMemoryForPurging,
             "EnableConfigurableThreadCacheMinCachedMemoryForPurging",
             base::FEATURE_DISABLED_BY_DEFAULT);

MIRACLE_PARAMETER_FOR_INT(
    GetThreadCacheMinCachedMemoryForPurgingBytes,
    kEnableConfigurableThreadCacheMinCachedMemoryForPurging,
    "ThreadCacheMinCachedMemoryForPurgingBytes",
    partition_alloc::kMinCachedMemoryForPurgingBytes)

// An apparent quarantine leak in the buffer partition unacceptably
// bloats memory when MiraclePtr is enabled in the renderer process.
// We believe we have found and patched the leak, but out of an
// abundance of caution, we provide this toggle that allows us to
// wholly disable MiraclePtr in the buffer partition, if necessary.
//
// TODO(crbug.com/40064499): this is unneeded once
// MiraclePtr-for-Renderer launches.
BASE_FEATURE(kPartitionAllocDisableBRPInBufferPartition,
             "PartitionAllocDisableBRPInBufferPartition",
             FEATURE_DISABLED_BY_DEFAULT);

#if PA_BUILDFLAG(USE_FREELIST_DISPATCHER)
BASE_FEATURE(kUsePoolOffsetFreelists,
             "PartitionAllocUsePoolOffsetFreelists",
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPartitionAllocMakeFreeNoOpOnShutdown,
             "PartitionAllocMakeFreeNoOpOnShutdown",
             FEATURE_DISABLED_BY_DEFAULT);

constexpr FeatureParam<WhenFreeBecomesNoOp>::Option
    kPartitionAllocMakeFreeNoOpOnShutdownOptions[] = {
        {WhenFreeBecomesNoOp::kBeforePreShutdown, "before-preshutdown"},
        {WhenFreeBecomesNoOp::kBeforeHaltingStartupTracingController,
         "before-halting-startup-tracing-controller"},
        {
            WhenFreeBecomesNoOp::kBeforeShutDownThreads,
            "before-shutdown-threads",
        },
        {
            WhenFreeBecomesNoOp::kInShutDownThreads,
            "in-shutdown-threads",
        },
        {
            WhenFreeBecomesNoOp::kAfterShutDownThreads,
            "after-shutdown-threads",
        },
};

const base::FeatureParam<WhenFreeBecomesNoOp>
    kPartitionAllocMakeFreeNoOpOnShutdownParam{
        &kPartitionAllocMakeFreeNoOpOnShutdown, "callsite",
        WhenFreeBecomesNoOp::kBeforeShutDownThreads,
        &kPartitionAllocMakeFreeNoOpOnShutdownOptions};

void MakeFreeNoOp(WhenFreeBecomesNoOp callsite) {
  CHECK(base::FeatureList::GetInstance());
  // Ignoring `free()` during Shutdown would allow developers to introduce new
  // dangling pointers. So we want to avoid ignoring free when it is enabled.
  // Note: For now, the DanglingPointerDetector is only enabled on 5 bots, and
  // on linux non-official configuration.
  // TODO(b/40802063): Reconsider this decision after the experiment.
#if PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
  if (base::FeatureList::IsEnabled(features::kPartitionAllocDanglingPtr)) {
    return;
  }
#endif  // PA_BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS)
#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
  if (base::FeatureList::IsEnabled(kPartitionAllocMakeFreeNoOpOnShutdown) &&
      kPartitionAllocMakeFreeNoOpOnShutdownParam.Get() == callsite) {
    allocator_shim::InsertNoOpOnFreeAllocatorShimOnShutDown();
  }
#endif  // PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
}

BASE_FEATURE(kPartitionAllocAdjustSizeWhenInForeground,
             "PartitionAllocAdjustSizeWhenInForeground",
#if BUILDFLAG(IS_MAC)
             FEATURE_ENABLED_BY_DEFAULT);
#else
             FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPartitionAllocUseSmallSingleSlotSpans,
             "PartitionAllocUseSmallSingleSlotSpans",
             base::FEATURE_DISABLED_BY_DEFAULT);

}  // namespace features
}  // namespace base
