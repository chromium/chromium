// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_alloc_features.h"

#include "base/base_export.h"
#include "base/feature_list.h"
#include "build/build_config.h"

namespace base {
namespace features {

const BASE_EXPORT Feature kPartitionAllocDanglingPtr{
    "PartitionAllocDanglingPtr", FEATURE_DISABLED_BY_DEFAULT};
constexpr FeatureParam<DanglingPtrMode>::Option kDanglingPtrModeOption[] = {
    {DanglingPtrMode::kCrash, "crash"},
    {DanglingPtrMode::kLogSignature, "log_signature"},
};
const base::FeatureParam<DanglingPtrMode> kDanglingPtrModeParam{
    &kPartitionAllocDanglingPtr,
    "mode",
    DanglingPtrMode::kCrash,
    &kDanglingPtrModeOption,
};

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

// If enabled, PCScan is turned on only for the renderer's malloc partition.
const Feature kPartitionAllocPCScanRendererOnly{
    "PartitionAllocPCScanRendererOnly", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, this instance belongs to the Control group of the BackupRefPtr
// binary experiment.
const Feature kPartitionAllocBackupRefPtrControl{
    "PartitionAllocBackupRefPtrControl", FEATURE_DISABLED_BY_DEFAULT};

// Use a larger maximum thread cache cacheable bucket size.
const Feature kPartitionAllocLargeThreadCacheSize{
  "PartitionAllocLargeThreadCacheSize",
#if BUILDFLAG(IS_ANDROID) && defined(ARCH_CPU_32_BITS)
      // Not unconditionally enabled on 32 bit Android, since it is a more
      // memory-constrained platform.
      FEATURE_DISABLED_BY_DEFAULT
#else
      FEATURE_ENABLED_BY_DEFAULT
#endif
};

const BASE_EXPORT Feature kPartitionAllocLargeEmptySlotSpanRing{
    "PartitionAllocLargeEmptySlotSpanRing", FEATURE_DISABLED_BY_DEFAULT};
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

const Feature kPartitionAllocBackupRefPtr {
  "PartitionAllocBackupRefPtr",
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_WIN) || \
    (BUILDFLAG(USE_ASAN_BACKUP_REF_PTR) && BUILDFLAG(IS_LINUX))
      FEATURE_ENABLED_BY_DEFAULT
#else
      FEATURE_DISABLED_BY_DEFAULT
#endif
};

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
        BackupRefPtrEnabledProcesses::kBrowserOnly,
        &kBackupRefPtrEnabledProcessesOptions};

constexpr FeatureParam<BackupRefPtrMode>::Option kBackupRefPtrModeOptions[] = {
    {BackupRefPtrMode::kDisabled, "disabled"},
    {BackupRefPtrMode::kEnabled, "enabled"},
    {BackupRefPtrMode::kEnabledWithoutZapping, "enabled-without-zapping"},
    {BackupRefPtrMode::kDisabledButSplitPartitions2Way,
     "disabled-but-2-way-split"},
    {BackupRefPtrMode::kDisabledButSplitPartitions3Way,
     "disabled-but-3-way-split"},
};

const base::FeatureParam<BackupRefPtrMode> kBackupRefPtrModeParam{
    &kPartitionAllocBackupRefPtr, "brp-mode", BackupRefPtrMode::kEnabled,
    &kBackupRefPtrModeOptions};

const base::FeatureParam<bool> kBackupRefPtrAsanEnableDereferenceCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-dereference-check", true};
const base::FeatureParam<bool> kBackupRefPtrAsanEnableExtractionCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-extraction-check",
    false};  // Not much noise at the moment to enable by default.
const base::FeatureParam<bool> kBackupRefPtrAsanEnableInstantiationCheckParam{
    &kPartitionAllocBackupRefPtr, "asan-enable-instantiation-check", true};

// If enabled, switches the bucket distribution to an alternate one. The
// alternate distribution must have buckets that are a subset of the default
// one.
const Feature kPartitionAllocUseAlternateDistribution{
    "PartitionAllocUseAlternateDistribution", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, switches PCScan scheduling to a mutator-aware scheduler. Does not
// affect whether PCScan is enabled itself.
const Feature kPartitionAllocPCScanMUAwareScheduler{
    "PartitionAllocPCScanMUAwareScheduler", FEATURE_ENABLED_BY_DEFAULT};

// If enabled, PCScan frees unconditionally all quarantined objects.
// This is a performance testing feature.
const Feature kPartitionAllocPCScanImmediateFreeing{
    "PartitionAllocPCScanImmediateFreeing", FEATURE_DISABLED_BY_DEFAULT};

// If enabled, PCScan clears eagerly (synchronously) on free().
const Feature kPartitionAllocPCScanEagerClearing{
    "PartitionAllocPCScanEagerClearing", FEATURE_DISABLED_BY_DEFAULT};

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

// Whether to sort the active slot spans in PurgeMemory().
extern const Feature kPartitionAllocSortActiveSlotSpans{
    "PartitionAllocSortActiveSlotSpans", FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace base
