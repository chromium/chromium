// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_features.h"

#include "base/allocator/partition_allocator/partition_alloc_base/time/time.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_root.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/metrics/field_trial_params.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "build/chromeos_buildflags.h"

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
        UnretainedDanglingPtrMode::kDumpWithoutCrashing,
        &kUnretainedDanglingPtrModeOption,
};

BASE_FEATURE(kPartitionAllocDanglingPtr,
             "PartitionAllocDanglingPtr",
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_FEATURE_FLAG) ||                   \
    (BUILDFLAG(ENABLE_DANGLING_RAW_PTR_CHECKS) && BUILDFLAG(IS_LINUX) && \
     !defined(OFFICIAL_BUILD) && (!defined(NDEBUG) || DCHECK_IS_ON()))
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

#if BUILDFLAG(USE_STARSCAN)
// If enabled, PCScan is turned on by default for all partitions that don't
// disable it explicitly.
BASE_FEATURE(kPartitionAllocPCScan,
             "PartitionAllocPCScan",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(USE_STARSCAN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
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

const base::FeatureParam<int> kPartitionAllocLargeThreadCacheSizeValue{
    &kPartitionAllocLargeThreadCacheSize,
    "PartitionAllocLargeThreadCacheSizeValue",
    ::partition_alloc::ThreadCacheLimits::kLargeSizeThreshold};

const base::FeatureParam<int>
    kPartitionAllocLargeThreadCacheSizeValueForLowRAMAndroid{
        &kPartitionAllocLargeThreadCacheSize,
        "PartitionAllocLargeThreadCacheSizeValueForLowRAMAndroid",
        ::partition_alloc::ThreadCacheLimits::kDefaultSizeThreshold};

BASE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing,
             "PartitionAllocLargeEmptySlotSpanRing",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

BASE_FEATURE(kPartitionAllocBackupRefPtr,
             "PartitionAllocBackupRefPtr",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH) || BUILDFLAG(IS_CHROMEOS_LACROS) ||     \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) ||                  \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocBackupRefPtrForAsh);
BASE_FEATURE(kPartitionAllocBackupRefPtrForAsh,
             "PartitionAllocBackupRefPtrForAsh",
             FEATURE_ENABLED_BY_DEFAULT);

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

constexpr FeatureParam<BackupRefPtrRefCountSize>::Option
    kBackupRefPtrRefCountSizeOptions[] = {
        {BackupRefPtrRefCountSize::kNatural, "natural"},
        {BackupRefPtrRefCountSize::k4B, "4B"},
        {BackupRefPtrRefCountSize::k8B, "8B"},
        {BackupRefPtrRefCountSize::k16B, "16B"}};

const base::FeatureParam<BackupRefPtrRefCountSize>
    kBackupRefPtrRefCountSizeParam{
        &kPartitionAllocBackupRefPtr, "ref-count-size",
        BackupRefPtrRefCountSize::kNatural, &kBackupRefPtrRefCountSizeOptions};

// Map -with-memory-reclaimer modes onto their counterpars without the suffix.
// They are the same, as memory reclaimer is now controlled independently.
// However, we need to keep both option strings, as there is a long tail of
// clients that may have an old field trial config, which used these modes.
//
// DO NOT USE -with-memory-reclaimer modes in new configs!
constexpr FeatureParam<BackupRefPtrMode>::Option kBackupRefPtrModeOptions[] = {
    {BackupRefPtrMode::kDisabled, "disabled"},
    {BackupRefPtrMode::kEnabled, "enabled"},
    {BackupRefPtrMode::kEnabled, "enabled-with-memory-reclaimer"},
    {BackupRefPtrMode::kDisabledButSplitPartitions2Way,
     "disabled-but-2-way-split"},
    {BackupRefPtrMode::kDisabledButSplitPartitions2Way,
     "disabled-but-2-way-split-with-memory-reclaimer"},
    {BackupRefPtrMode::kDisabledButSplitPartitions3Way,
     "disabled-but-3-way-split"},
};

const base::FeatureParam<BackupRefPtrMode> kBackupRefPtrModeParam{
    &kPartitionAllocBackupRefPtr, "brp-mode", BackupRefPtrMode::kEnabled,
    &kBackupRefPtrModeOptions};

BASE_FEATURE(kPartitionAllocMemoryTagging,
             "PartitionAllocMemoryTagging",
             FEATURE_DISABLED_BY_DEFAULT);

constexpr FeatureParam<MemtagMode>::Option kMemtagModeOptions[] = {
    {MemtagMode::kSync, "sync"},
    {MemtagMode::kAsync, "async"}};

const base::FeatureParam<MemtagMode> kMemtagModeParam{
    &kPartitionAllocMemoryTagging, "memtag-mode", MemtagMode::kAsync,
    &kMemtagModeOptions};

constexpr FeatureParam<MemoryTaggingEnabledProcesses>::Option
    kMemoryTaggingEnabledProcessesOptions[] = {
        {MemoryTaggingEnabledProcesses::kBrowserOnly, "browser-only"},
        {MemoryTaggingEnabledProcesses::kNonRenderer, "non-renderer"},
        {MemoryTaggingEnabledProcesses::kAllProcesses, "all-processes"}};

const base::FeatureParam<MemoryTaggingEnabledProcesses>
    kMemoryTaggingEnabledProcessesParam{
        &kPartitionAllocMemoryTagging, "enabled-processes",
        MemoryTaggingEnabledProcesses::kBrowserOnly,
        &kMemoryTaggingEnabledProcessesOptions};

BASE_FEATURE(kKillPartitionAllocMemoryTagging,
             "KillPartitionAllocMemoryTagging",
             FEATURE_DISABLED_BY_DEFAULT);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPermissiveMte);
BASE_FEATURE(kPartitionAllocPermissiveMte,
             "PartitionAllocPermissiveMte",
             FEATURE_ENABLED_BY_DEFAULT);

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
#if BUILDFLAG(PCSCAN_STACK_SUPPORTED)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif  // BUILDFLAG(PCSCAN_STACK_SUPPORTED)
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

#if BUILDFLAG(IS_ANDROID)
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

const base::FeatureParam<double> kThreadCacheMultiplier{
    &kEnableConfigurableThreadCacheMultiplier, "ThreadCacheMultiplier", 2.};

const base::FeatureParam<double> kThreadCacheMultiplierForAndroid{
    &kEnableConfigurableThreadCacheMultiplier,
    "ThreadCacheMultiplierForAndroid", 1.};

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

const base::FeatureParam<base::TimeDelta> kThreadCacheMinPurgeInterval{
    &kEnableConfigurableThreadCachePurgeInterval, "ThreadCacheMinPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kMinPurgeInterval)};

const base::FeatureParam<base::TimeDelta> kThreadCacheMaxPurgeInterval{
    &kEnableConfigurableThreadCachePurgeInterval, "ThreadCacheMaxPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kMaxPurgeInterval)};

const base::FeatureParam<base::TimeDelta> kThreadCacheDefaultPurgeInterval{
    &kEnableConfigurableThreadCachePurgeInterval,
    "ThreadCacheDefaultPurgeInterval",
    FromPartitionAllocTimeDelta(partition_alloc::kDefaultPurgeInterval)};

const partition_alloc::internal::base::TimeDelta
GetThreadCacheMinPurgeInterval() {
  return ToPartitionAllocTimeDelta(kThreadCacheMinPurgeInterval.Get());
}

const partition_alloc::internal::base::TimeDelta
GetThreadCacheMaxPurgeInterval() {
  return ToPartitionAllocTimeDelta(kThreadCacheMaxPurgeInterval.Get());
}

const partition_alloc::internal::base::TimeDelta
GetThreadCacheDefaultPurgeInterval() {
  return ToPartitionAllocTimeDelta(kThreadCacheDefaultPurgeInterval.Get());
}

BASE_FEATURE(kEnableConfigurableThreadCacheMinCachedMemoryForPurging,
             "EnableConfigurableThreadCacheMinCachedMemoryForPurging",
             base::FEATURE_DISABLED_BY_DEFAULT);

const base::FeatureParam<int> kThreadCacheMinCachedMemoryForPurgingBytes{
    &kEnableConfigurableThreadCacheMinCachedMemoryForPurging,
    "ThreadCacheMinCachedMemoryForPurgingBytes",
    partition_alloc::kMinCachedMemoryForPurgingBytes};

}  // namespace features
}  // namespace base
