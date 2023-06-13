// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_features.h"

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/base_export.h"
#include "base/feature_list.h"
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
#if BUILDFLAG(ENABLE_DANGLING_RAW_PTR_FEATURE_FLAG)
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
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
             // Not unconditionally enabled on 32 bit Android, since it is a
             // more memory-constrained platform.
             FEATURE_DISABLED_BY_DEFAULT
#else
             FEATURE_ENABLED_BY_DEFAULT
#endif
);

BASE_FEATURE(kPartitionAllocLargeEmptySlotSpanRing,
             "PartitionAllocLargeEmptySlotSpanRing",
             FEATURE_DISABLED_BY_DEFAULT);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

BASE_FEATURE(kPartitionAllocBackupRefPtr,
             "PartitionAllocBackupRefPtr",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_MAC) || \
    BUILDFLAG(IS_CHROMEOS_ASH) ||                                      \
    (BUILDFLAG(IS_LINUX) && !BUILDFLAG(IS_CASTOS)) ||                  \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG) ||                   \
    (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX))
             FEATURE_ENABLED_BY_DEFAULT
#else
             FEATURE_DISABLED_BY_DEFAULT
#endif
);

BASE_EXPORT BASE_DECLARE_FEATURE(kPartitionAllocBackupRefPtrForAsh);
BASE_FEATURE(kPartitionAllocBackupRefPtrForAsh,
             "PartitionAllocBackupRefPtrForAsh",
             FEATURE_DISABLED_BY_DEFAULT);

constexpr FeatureParam<BackupRefPtrEnabledProcesses>::Option
    kBackupRefPtrEnabledProcessesOptions[] = {
        {BackupRefPtrEnabledProcesses::kBrowserOnly, "browser-only"},
        {BackupRefPtrEnabledProcesses::kBrowserAndRenderer,
         "browser-and-renderer"},
        {BackupRefPtrEnabledProcesses::kNonRenderer, "non-renderer"},
        {BackupRefPtrEnabledProcesses::kAllProcesses, "all-processes"}};

const base::FeatureParam<BackupRefPtrEnabledProcesses>
    kBackupRefPtrEnabledProcessesParam {
  &kPartitionAllocBackupRefPtr, "enabled-processes",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) ||    \
    BUILDFLAG(ENABLE_BACKUP_REF_PTR_FEATURE_FLAG) || \
    (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX))
      BackupRefPtrEnabledProcesses::kNonRenderer,
#else
      BackupRefPtrEnabledProcesses::kBrowserOnly,
#endif
      &kBackupRefPtrEnabledProcessesOptions
};

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

constexpr FeatureParam<BackupRefPtrMode>::Option kBackupRefPtrModeOptions[] = {
    {BackupRefPtrMode::kDisabled, "disabled"},
    {BackupRefPtrMode::kEnabled, "enabled"},
    {BackupRefPtrMode::kEnabledWithMemoryReclaimer,
     "enabled-with-memory-reclaimer"},
    {BackupRefPtrMode::kDisabledButSplitPartitions2Way,
     "disabled-but-2-way-split"},
    {BackupRefPtrMode::kDisabledButSplitPartitions2WayWithMemoryReclaimer,
     "disabled-but-2-way-split-with-memory-reclaimer"},
    {BackupRefPtrMode::kDisabledButSplitPartitions3Way,
     "disabled-but-3-way-split"},
};

const base::FeatureParam<BackupRefPtrMode> kBackupRefPtrModeParam{
    &kPartitionAllocBackupRefPtr, "brp-mode",
    BackupRefPtrMode::kEnabledWithMemoryReclaimer, &kBackupRefPtrModeOptions};

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

}  // namespace features
}  // namespace base
