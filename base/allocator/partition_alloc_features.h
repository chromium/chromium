// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"
#include "base/metrics/field_trial_params.h"

namespace base {

struct Feature;

namespace features {

#if defined(PA_ALLOW_PCSCAN)
extern const BASE_EXPORT Feature kPartitionAllocPCScan;
#endif  // defined(PA_ALLOW_PCSCAN)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
extern const BASE_EXPORT Feature kPartitionAllocPCScanBrowserOnly;
extern const BASE_EXPORT Feature kPartitionAllocPCScanRendererOnly;
extern const BASE_EXPORT Feature kPartitionAllocBackupRefPtrControl;
extern const BASE_EXPORT Feature kPartitionAllocLargeThreadCacheSize;
extern const BASE_EXPORT Feature kPartitionAllocLargeEmptySlotSpanRing;

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

  // BRP is disabled, but the main partition is split out, as if BRP was enabled
  // in the "previous slot" mode.
  kDisabledButSplitPartitions2Way,

  // BRP is disabled, but the main partition *and* aligned partition are split
  // out, as if BRP was enabled in the "before allocation" mode.
  kDisabledButSplitPartitions3Way,
};

extern const BASE_EXPORT Feature kPartitionAllocBackupRefPtr;
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrEnabledProcesses>
    kBackupRefPtrEnabledProcessesParam;
extern const BASE_EXPORT base::FeatureParam<BackupRefPtrMode>
    kBackupRefPtrModeParam;
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

extern const BASE_EXPORT Feature kPartitionAllocPCScanMUAwareScheduler;
extern const BASE_EXPORT Feature kPartitionAllocPCScanStackScanning;
extern const BASE_EXPORT Feature kPartitionAllocDCScan;
extern const BASE_EXPORT Feature kPartitionAllocPCScanImmediateFreeing;
extern const BASE_EXPORT Feature kPartitionAllocPCScanEagerClearing;

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
