// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/base_export.h"
#include "base/feature_list.h"

namespace base {

namespace features {

#if PA_ALLOW_PCSCAN
extern const BASE_EXPORT Feature kPartitionAllocPCScan;
#endif  // PA_ALLOW_PCSCAN
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
extern const BASE_EXPORT Feature kPartitionAllocPCScanBrowserOnly;
extern const BASE_EXPORT Feature kPartitionAllocBackupRefPtrControl;
extern const BASE_EXPORT Feature kPartitionAllocThreadCachePeriodicPurge;
extern const BASE_EXPORT Feature kPartitionAllocLargeThreadCacheSize;
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

extern const BASE_EXPORT Feature kPartitionAllocPCScanMUAwareScheduler;
extern const BASE_EXPORT Feature kPartitionAllocPCScanStackScanning;

extern const BASE_EXPORT Feature kPartitionAllocLazyCommit;

// TODO(bartekn): Clean up.
ALWAYS_INLINE bool IsPartitionAllocGigaCageEnabled() {
  return true;
}

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
