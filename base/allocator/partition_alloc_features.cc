// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/allocator/partition_alloc_features.h"

#include "base/base_export.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/shim/allocator_shim_dispatch_to_noop_on_free.h"

namespace base::features {

namespace {

static constexpr char kPAFeatureEnabledProcessesStr[] = "enabled-processes";
static constexpr char kBrowserOnlyStr[] = "browser-only";
static constexpr char kBrowserAndRendererStr[] = "browser-and-renderer";
static constexpr char kNonRendererStr[] = "non-renderer";
static constexpr char kAllProcessesStr[] = "all-processes";

}  // namespace

BASE_FEATURE(kPartitionAllocUnretainedDanglingPtr, FEATURE_ENABLED_BY_DEFAULT);

constexpr FeatureParam<UnretainedDanglingPtrMode>::Option
    kUnretainedDanglingPtrModeOption[] = {
        {UnretainedDanglingPtrMode::kCrash, "crash"},
        {UnretainedDanglingPtrMode::kDumpWithoutCrashing,
         "dump_without_crashing"},
};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<UnretainedDanglingPtrMode>
    kUnretainedDanglingPtrModeParam = {
        &kPartitionAllocUnretainedDanglingPtr,
        "mode",
        UnretainedDanglingPtrMode::kCrash,
        &kUnretainedDanglingPtrModeOption,
};

// Note: DPD conflicts with no-op `free()` (see
// `base::allocator::MakeFreeNoOp()`). No-op `free()` stands down in the
// presence of DPD, but hypothetically fully launching DPD should prompt
// a rethink of no-op `free()`.
BASE_FEATURE(kPartitionAllocDanglingPtr,
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
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<DanglingPtrMode> kDanglingPtrModeParam{
    &kPartitionAllocDanglingPtr,
    "mode",
    DanglingPtrMode::kCrash,
    &kDanglingPtrModeOption,
};
constexpr FeatureParam<DanglingPtrType>::Option kDanglingPtrTypeOption[] = {
    {DanglingPtrType::kAll, "all"},
    {DanglingPtrType::kCrossTask, "cross_task"},
};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<DanglingPtrType> kDanglingPtrTypeParam{
    &kPartitionAllocDanglingPtr,
    "type",
    DanglingPtrType::kAll,
    &kDanglingPtrTypeOption,
};

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Use a larger maximum thread cache cacheable bucket size.
BASE_FEATURE(kPartitionAllocLargeThreadCacheSize, FEATURE_ENABLED_BY_DEFAULT);

BASE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             FEATURE_ENABLED_BY_DEFAULT);
#else
             FEATURE_DISABLED_BY_DEFAULT);
#endif

BASE_FEATURE(kPartitionAllocWithAdvancedChecks, FEATURE_DISABLED_BY_DEFAULT);
constexpr FeatureParam<PartitionAllocWithAdvancedChecksEnabledProcesses>::Option
    kPartitionAllocWithAdvancedChecksEnabledProcessesOptions[] = {
        {PartitionAllocWithAdvancedChecksEnabledProcesses::kBrowserOnly,
         kBrowserOnlyStr},
        {PartitionAllocWithAdvancedChecksEnabledProcesses::kBrowserAndRenderer,
         kBrowserAndRendererStr},
        {PartitionAllocWithAdvancedChecksEnabledProcesses::kNonRenderer,
         kNonRendererStr},
        {PartitionAllocWithAdvancedChecksEnabledProcesses::kAllProcesses,
         kAllProcessesStr}};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<PartitionAllocWithAdvancedChecksEnabledProcesses>
    kPartitionAllocWithAdvancedChecksEnabledProcessesParam{
        &kPartitionAllocWithAdvancedChecks, kPAFeatureEnabledProcessesStr,
        PartitionAllocWithAdvancedChecksEnabledProcesses::kBrowserOnly,
        &kPartitionAllocWithAdvancedChecksEnabledProcessesOptions};

BASE_FEATURE(kPartitionAllocSchedulerLoopQuarantine,
             FEATURE_DISABLED_BY_DEFAULT);
// Scheduler Loop Quarantine's config.
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<std::string>
    kPartitionAllocSchedulerLoopQuarantineConfig{
        &kPartitionAllocSchedulerLoopQuarantine,
        "PartitionAllocSchedulerLoopQuarantineConfig", "{}"};

BASE_FEATURE(kPartitionAllocSchedulerLoopQuarantineTaskControlledPurge,
             FEATURE_DISABLED_BY_DEFAULT);
constexpr FeatureParam<
    PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses>::Option
    kPartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcessesOptions[] =
        {{PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses::
              kBrowserOnly,
          kBrowserOnlyStr},
         {PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses::
              kBrowserAndRenderer,
          kBrowserAndRendererStr},
         {PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses::
              kNonRenderer,
          kNonRendererStr},
         {PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses::
              kAllProcesses,
          kAllProcessesStr}};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<
    PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses>
    kPartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcessesParam{
        &kPartitionAllocSchedulerLoopQuarantineTaskControlledPurge,
        "PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcess"
        "es",
        PartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcesses::
            kBrowserOnly,
        &kPartitionAllocSchedulerLoopQuarantineTaskControlledPurgeEnabledProcessesOptions};

BASE_FEATURE(kPartitionAllocEventuallyZeroFreedMemory,
             FEATURE_DISABLED_BY_DEFAULT);

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

BASE_FEATURE(kPartitionAllocBackupRefPtr,
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr FeatureParam<BackupRefPtrEnabledProcesses>::Option
    kBackupRefPtrEnabledProcessesOptions[] = {
        {BackupRefPtrEnabledProcesses::kBrowserOnly, kBrowserOnlyStr},
        {BackupRefPtrEnabledProcesses::kBrowserAndRenderer,
         kBrowserAndRendererStr},
        {BackupRefPtrEnabledProcesses::kNonRenderer, kNonRendererStr},
        {BackupRefPtrEnabledProcesses::kAllProcesses, kAllProcessesStr}};

BASE_FEATURE_ENUM_PARAM(BackupRefPtrEnabledProcesses,
                        kBackupRefPtrEnabledProcessesParam,
                        &kPartitionAllocBackupRefPtr,
                        kPAFeatureEnabledProcessesStr,
#if PA_BUILDFLAG(IS_ANDROID)
                        BackupRefPtrEnabledProcesses::kNonRenderer,
#else
                        BackupRefPtrEnabledProcesses::kAllProcesses,
#endif
                        &kBackupRefPtrEnabledProcessesOptions);

constexpr FeatureParam<BackupRefPtrMode>::Option kBackupRefPtrModeOptions[] = {
    {BackupRefPtrMode::kDisabled, "disabled"},
    {BackupRefPtrMode::kEnabled, "enabled"},
};

BASE_FEATURE_ENUM_PARAM(BackupRefPtrMode,
                        kBackupRefPtrModeParam,
                        &kPartitionAllocBackupRefPtr,
                        "brp-mode",
                        BackupRefPtrMode::kEnabled,
                        &kBackupRefPtrModeOptions);
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<int> kBackupRefPtrExtraExtrasSizeParam{
    &kPartitionAllocBackupRefPtr, "brp-extra-extras-size", 0};
constinit const FeatureParam<bool> kBackupRefPtrSuppressDoubleFreeDetectedCrash{
    &kPartitionAllocBackupRefPtr, "brp-suppress-double-free-detected-crash",
    false};
constinit const FeatureParam<bool> kBackupRefPtrSuppressCorruptionDetectedCrash{
    &kPartitionAllocBackupRefPtr, "brp-suppress-corruption-detected-crash",
#if PA_BUILDFLAG(IS_IOS)
    // TODO(crbug.com/41497028): Continue investigation and remove once
    // addressed.
    true};
#else
    false};
#endif

BASE_FEATURE(kPartitionAllocMemoryTagging,
#if PA_BUILDFLAG(USE_FULL_MTE) || BUILDFLAG(IS_ANDROID)
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

constexpr FeatureParam<MemtagMode>::Option kMemtagModeOptions[] = {
    {MemtagMode::kSync, "sync"},
    {MemtagMode::kAsync, "async"}};

// Note: Do not use the prepared muacro as of no need for a local cache.
constinit const FeatureParam<MemtagMode> kMemtagModeParam{
    &kPartitionAllocMemoryTagging, "memtag-mode",
#if PA_BUILDFLAG(USE_FULL_MTE)
    MemtagMode::kSync,
#else
    MemtagMode::kAsync,
#endif
    &kMemtagModeOptions};

constexpr FeatureParam<RetagMode>::Option kRetagModeOptions[] = {
    {RetagMode::kIncrement, "increment"},
    {RetagMode::kRandom, "random"},
};

// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<RetagMode> kRetagModeParam{
    &kPartitionAllocMemoryTagging, "retag-mode", RetagMode::kIncrement,
    &kRetagModeOptions};

constexpr FeatureParam<MemoryTaggingEnabledProcesses>::Option
    kMemoryTaggingEnabledProcessesOptions[] = {
        {MemoryTaggingEnabledProcesses::kBrowserOnly, kBrowserOnlyStr},
        {MemoryTaggingEnabledProcesses::kNonRenderer, kNonRendererStr},
        {MemoryTaggingEnabledProcesses::kAllProcesses, kAllProcessesStr}};

// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<MemoryTaggingEnabledProcesses>
    kMemoryTaggingEnabledProcessesParam{
        &kPartitionAllocMemoryTagging, kPAFeatureEnabledProcessesStr,
#if PA_BUILDFLAG(USE_FULL_MTE)
        MemoryTaggingEnabledProcesses::kAllProcesses,
#else
        MemoryTaggingEnabledProcesses::kNonRenderer,
#endif
        &kMemoryTaggingEnabledProcessesOptions};

BASE_FEATURE(kKillPartitionAllocMemoryTagging, FEATURE_DISABLED_BY_DEFAULT);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocPermissiveMte);
BASE_FEATURE(kPartitionAllocPermissiveMte,
#if PA_BUILDFLAG(USE_FULL_MTE)
             // We want to actually crash if USE_FULL_MTE is enabled.
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kAsanBrpDereferenceCheck, FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE(kAsanBrpExtractionCheck,       // Not much noise at the moment to
             FEATURE_DISABLED_BY_DEFAULT);  // enable by default.
BASE_FEATURE(kAsanBrpInstantiationCheck, FEATURE_ENABLED_BY_DEFAULT);

// If enabled, switches the bucket distribution to a denser one.
//
// We enable this by default everywhere except for 32-bit Android, since we saw
// regressions there.
BASE_FEATURE(kPartitionAllocUseDenserDistribution,
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
);
const FeatureParam<BucketDistributionMode>::Option
    kPartitionAllocBucketDistributionOption[] = {
        {BucketDistributionMode::kDefault, "default"},
        {BucketDistributionMode::kDenser, "denser"},
};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<BucketDistributionMode>
    kPartitionAllocBucketDistributionParam{
        &kPartitionAllocUseDenserDistribution, "mode",
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
        BucketDistributionMode::kDefault,
#else
        BucketDistributionMode::kDenser,
#endif  // BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
        &kPartitionAllocBucketDistributionOption};

BASE_FEATURE(kPartitionAllocMemoryReclaimer, FEATURE_ENABLED_BY_DEFAULT);
BASE_FEATURE_PARAM(TimeDelta,
                   kPartitionAllocMemoryReclaimerInterval,
                   &kPartitionAllocMemoryReclaimer,
                   "interval",
                   TimeDelta()  // Defaults to zero.
);

// Configures whether we set a lower limit for renderers that do not have a main
// frame, similar to the limit that is already done for backgrounded renderers.
BASE_FEATURE(kLowerPAMemoryLimitForNonMainRenderers,
             FEATURE_DISABLED_BY_DEFAULT);

// Whether to straighten free lists for larger slot spans in PurgeMemory() ->
// ... -> PartitionPurgeSlotSpan().
BASE_FEATURE(kPartitionAllocStraightenLargerSlotSpanFreeLists,
             FEATURE_ENABLED_BY_DEFAULT);
const FeatureParam<partition_alloc::StraightenLargerSlotSpanFreeListsMode>::
    Option kPartitionAllocStraightenLargerSlotSpanFreeListsModeOption[] = {
        {partition_alloc::StraightenLargerSlotSpanFreeListsMode::
             kOnlyWhenUnprovisioning,
         "only-when-unprovisioning"},
        {partition_alloc::StraightenLargerSlotSpanFreeListsMode::kAlways,
         "always"},
};
// Note: Do not use the prepared macro as of no need for a local cache.
constinit const FeatureParam<
    partition_alloc::StraightenLargerSlotSpanFreeListsMode>
    kPartitionAllocStraightenLargerSlotSpanFreeListsMode = {
        &kPartitionAllocStraightenLargerSlotSpanFreeLists,
        "mode",
        partition_alloc::StraightenLargerSlotSpanFreeListsMode::
            kOnlyWhenUnprovisioning,
        &kPartitionAllocStraightenLargerSlotSpanFreeListsModeOption,
};

// Whether to sort free lists for smaller slot spans in PurgeMemory().
BASE_FEATURE(kPartitionAllocSortSmallerSlotSpanFreeLists,
             FEATURE_ENABLED_BY_DEFAULT);

// Whether to sort the active slot spans in PurgeMemory().
BASE_FEATURE(kPartitionAllocSortActiveSlotSpans, FEATURE_DISABLED_BY_DEFAULT);

#if BUILDFLAG(IS_WIN)
// Whether to retry allocations when commit fails.
BASE_FEATURE(kPageAllocatorRetryOnCommitFailure, FEATURE_DISABLED_BY_DEFAULT);
#endif

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_CHROMEOS)
// A parameter to exclude or not exclude PartitionAllocSupport from
// PartialLowModeOnMidRangeDevices. This is used to see how it affects
// renderer performances, e.g. blink_perf.parser benchmark.
// The feature: kPartialLowEndModeOnMidRangeDevices is defined in
// //base/features.cc. Since the following feature param is related to
// PartitionAlloc, define the param here.
BASE_FEATURE_PARAM(bool,
                   kPartialLowEndModeExcludePartitionAllocSupport,
                   &kPartialLowEndModeOnMidRangeDevices,
                   "exclude-partition-alloc-support",
                   false);
#endif

constexpr partition_alloc::internal::base::TimeDelta ToPartitionAllocTimeDelta(
    TimeDelta time_delta) {
  return partition_alloc::internal::base::Microseconds(
      time_delta.InMicroseconds());
}

constexpr TimeDelta FromPartitionAllocTimeDelta(
    partition_alloc::internal::base::TimeDelta time_delta) {
  return Microseconds(time_delta.InMicroseconds());
}

BASE_FEATURE(kPartitionAllocAdjustSizeWhenInForeground,
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
             FEATURE_ENABLED_BY_DEFAULT);
#else
             FEATURE_DISABLED_BY_DEFAULT);
#endif

#if PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)
BASE_FEATURE(kPartitionAllocUsePriorityInheritanceLocks,
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_PRIORITY_INHERITANCE)

BASE_FEATURE(kPartitionAllocFreeWithSize, FEATURE_DISABLED_BY_DEFAULT);

}  // namespace base::features
