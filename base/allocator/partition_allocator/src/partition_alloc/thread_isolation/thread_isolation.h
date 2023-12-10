// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_THREAD_ISOLATION_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_THREAD_ISOLATION_H_

#include "partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include <cstddef>
#include <cstdint>

#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"

#if BUILDFLAG(ENABLE_PKEYS)
#include "partition_alloc/thread_isolation/pkey.h"
#endif

#if !BUILDFLAG(HAS_64_BIT_POINTERS)
#error "thread isolation support requires 64 bit pointers"
#endif

namespace partition_alloc {

struct ThreadIsolationOption {
  constexpr ThreadIsolationOption() = default;
  explicit ThreadIsolationOption(bool enabled) : enabled(enabled) {}

#if BUILDFLAG(ENABLE_PKEYS)
  explicit ThreadIsolationOption(int pkey) : pkey(pkey) {
    enabled = pkey != internal::kInvalidPkey;
  }
  int pkey = -1;
#endif  // BUILDFLAG(ENABLE_PKEYS)

  bool enabled = false;

  bool operator==(const ThreadIsolationOption& other) const {
#if BUILDFLAG(ENABLE_PKEYS)
    if (pkey != other.pkey) {
      return false;
    }
#endif  // BUILDFLAG(ENABLE_PKEYS)
    return enabled == other.enabled;
  }
};

}  // namespace partition_alloc

namespace partition_alloc::internal {

#if BUILDFLAG(PA_DCHECK_IS_ON)

struct PA_THREAD_ISOLATED_ALIGN ThreadIsolationSettings {
  bool enabled = false;
  static ThreadIsolationSettings settings PA_CONSTINIT;
};

#if BUILDFLAG(ENABLE_PKEYS)

using LiftThreadIsolationScope = LiftPkeyRestrictionsScope;

#endif  // BUILDFLAG(ENABLE_PKEYS)
#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption thread_isolation);
void UnprotectThreadIsolatedGlobals();
[[nodiscard]] int MprotectWithThreadIsolation(
    void* addr,
    size_t len,
    int prot,
    ThreadIsolationOption thread_isolation);

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_THREAD_ISOLATION_H_
