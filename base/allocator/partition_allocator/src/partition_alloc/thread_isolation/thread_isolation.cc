// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/thread_isolation.h"

#if BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include "base/allocator/partition_allocator/src/partition_alloc/address_pool_manager.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_check.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/src/partition_alloc/reservation_offset_table.h"

#if BUILDFLAG(ENABLE_PKEYS)
#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/pkey.h"
#endif

namespace partition_alloc::internal {

#if BUILDFLAG(PA_DCHECK_IS_ON)
ThreadIsolationSettings ThreadIsolationSettings::settings;
#endif

void WriteProtectThreadIsolatedMemory(ThreadIsolationOption thread_isolation,
                                      void* address,
                                      size_t size) {
  PA_DCHECK((reinterpret_cast<uintptr_t>(address) &
             PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) == 0);
#if BUILDFLAG(ENABLE_PKEYS)
  partition_alloc::internal::TagMemoryWithPkey(
      thread_isolation.enabled ? thread_isolation.pkey : kDefaultPkey, address,
      size);
#else
#error unexpected thread isolation mode
#endif
}

template <typename T>
void WriteProtectThreadIsolatedVariable(ThreadIsolationOption thread_isolation,
                                        T& var,
                                        size_t offset = 0) {
  WriteProtectThreadIsolatedMemory(thread_isolation, (char*)&var + offset,
                                   sizeof(T) - offset);
}

int MprotectWithThreadIsolation(void* addr,
                                size_t len,
                                int prot,
                                ThreadIsolationOption thread_isolation) {
#if BUILDFLAG(ENABLE_PKEYS)
  return PkeyMprotect(addr, len, prot, thread_isolation.pkey);
#endif
}

void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption thread_isolation) {
  WriteProtectThreadIsolatedVariable(thread_isolation,
                                     PartitionAddressSpace::setup_);

  AddressPoolManager::Pool* pool =
      AddressPoolManager::GetInstance().GetPool(kThreadIsolatedPoolHandle);
  WriteProtectThreadIsolatedVariable(
      thread_isolation, *pool,
      offsetof(AddressPoolManager::Pool, alloc_bitset_));

  uint16_t* pkey_reservation_offset_table =
      GetReservationOffsetTable(kThreadIsolatedPoolHandle);
  WriteProtectThreadIsolatedMemory(
      thread_isolation, pkey_reservation_offset_table,
      ReservationOffsetTable::kReservationOffsetTableLength);

#if BUILDFLAG(PA_DCHECK_IS_ON)
  WriteProtectThreadIsolatedVariable(thread_isolation,
                                     ThreadIsolationSettings::settings);
#endif
}

void UnprotectThreadIsolatedGlobals() {
  WriteProtectThreadIsolatedGlobals(ThreadIsolationOption(false));
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_THREAD_ISOLATION)
