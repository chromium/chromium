// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/base_export.h"
#include "base/compiler_specific.h"

namespace base {

struct Feature;

namespace features {

#if defined(PA_ALLOW_PCSCAN)
extern const BASE_EXPORT Feature kPartitionAllocPCScan;
#endif  // defined(PA_ALLOW_PCSCAN)
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
extern const BASE_EXPORT Feature kPartitionAllocPCScanBrowserOnly;
extern const BASE_EXPORT Feature kPartitionAllocBackupRefPtrControl;
extern const BASE_EXPORT Feature kPartitionAllocThreadCachePeriodicPurge;
extern const BASE_EXPORT Feature kPartitionAllocLargeThreadCacheSize;
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

extern const BASE_EXPORT Feature kPartitionAllocPCScanMUAwareScheduler;
extern const BASE_EXPORT Feature kPartitionAllocPCScanStackScanning;
extern const BASE_EXPORT Feature kPartitionAllocDCScan;
extern const BASE_EXPORT Feature kPartitionAllocPCScanImmediateFreeing;

extern const BASE_EXPORT Feature kPartitionAllocLazyCommit;

}  // namespace features
}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_FEATURES_H_
