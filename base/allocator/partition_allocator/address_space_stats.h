// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_SPACE_STATS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_SPACE_STATS_H_

#include <cstddef>

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"

namespace partition_alloc {

// All members are measured in super pages.
struct PoolStats {
  size_t usage = 0;

  // On 32-bit, pools are mainly logical entities, intermingled with
  // allocations not managed by PartitionAlloc. The "largest available
  // reservation" is not possible to measure in that case.
#if defined(PA_HAS_64_BITS_POINTERS)
  size_t largest_available_reservation = 0;
#endif  // defined(PA_HAS_64_BITS_POINTERS)
};

struct AddressSpaceStats {
  PoolStats regular_pool_stats;
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PoolStats brp_pool_stats;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#if defined(PA_HAS_64_BITS_POINTERS)
  PoolStats configurable_pool_stats;
#else
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  size_t blocklist_size;  // measured in super pages
  size_t blocklist_hit_count;
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
#endif  // defined(PA_HAS_64_BITS_POINTERS)
#if BUILDFLAG(ENABLE_PKEYS)
  PoolStats pkey_pool_stats;
#endif
};

// Interface passed to `AddressPoolManager::DumpStats()` to mediate
// for `AddressSpaceDumpProvider`.
class PA_COMPONENT_EXPORT(PARTITION_ALLOC) AddressSpaceStatsDumper {
 public:
  virtual void DumpStats(const AddressSpaceStats* address_space_stats) = 0;
};

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_ADDRESS_SPACE_STATS_H_
