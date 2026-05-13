// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_THREAD_CACHE_H_
#define PARTITION_ALLOC_THREAD_CACHE_H_

#include <cstdint>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"

#if PA_BUILDFLAG(PA_ARCH_CPU_X86_64) && PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#endif

namespace partition_alloc {

namespace tools {

// This is used from ThreadCacheInspector, which runs in a different process. It
// scans the process memory looking for the two needles, to locate the thread
// cache registry instance.
//
// These two values were chosen randomly, and in particular neither is a valid
// pointer on most 64 bit architectures.
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
constexpr uintptr_t kNeedle1 = 0xe69e32f3ad9ea63;
constexpr uintptr_t kNeedle2 = 0x9615ee1c5eb14caf;
#else
constexpr uintptr_t kNeedle1 = 0xe69e32f3;
constexpr uintptr_t kNeedle2 = 0x9615ee1c;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

// This array contains, in order:
// - kNeedle1
// - &ThreadCacheRegistry::Instance()
// - kNeedle2
//
// It is refererenced in the thread cache constructor to make sure it is not
// removed by the compiler. It is also not const to make sure it ends up in
// .data.
constexpr size_t kThreadCacheNeedleArraySize = 4;
extern uintptr_t kThreadCacheNeedleArray[kThreadCacheNeedleArraySize];

}  // namespace tools

namespace internal {

constexpr inline size_t kInvalidThreadCacheIndex = static_cast<size_t>(-1);

}  // namespace internal

// Static only struct, working as a wrapper of internal::ThreadCache and
// internal::ThreadCacheRegistry.
struct ThreadCache {
  static constexpr float kDefaultMultiplier = 2.;

  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  static int64_t GetPeriodicPurgeNextIntervalInMicroseconds();
  PA_COMPONENT_EXPORT(PARTITION_ALLOC) static void RunPeriodicPurge();

  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  static void SetThreadCacheMultiplier(float multiplier);
  PA_COMPONENT_EXPORT(PARTITION_ALLOC)
  static void SetLargestCachedSize(size_t size);

  // Purge the thread cache of the current thread, if one exists.
  PA_COMPONENT_EXPORT(PARTITION_ALLOC) static void PurgeCurrentThread();
  PA_COMPONENT_EXPORT(PARTITION_ALLOC) static void PurgeAllThread();
};

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_THREAD_CACHE_H_
