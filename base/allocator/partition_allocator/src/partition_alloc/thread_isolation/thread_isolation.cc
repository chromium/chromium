// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/thread_isolation/thread_isolation.h"

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/reservation_offset_table.h"

#if PA_BUILDFLAG(ENABLE_PKEYS)
#include "partition_alloc/thread_isolation/pkey.h"
#endif

namespace partition_alloc::internal {

#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_PARTITION_LOCK_REENTRANCY_CHECK)
PA_CONSTINIT ThreadIsolationSettings ThreadIsolationSettings::settings;
#endif

void WriteProtectThreadIsolatedMemory(ThreadIsolationOption thread_isolation,
                                      void* address,
                                      size_t size,
                                      bool read_only = false) {
  PA_DCHECK((reinterpret_cast<uintptr_t>(address) &
             PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) == 0);
  if (read_only) {
    SetSystemPagesAccess(
        address, size,
        PageAccessibilityConfiguration(
            thread_isolation.enabled
                ? PageAccessibilityConfiguration::Permissions::kRead
                : PageAccessibilityConfiguration::Permissions::kReadWrite));
    return;
  }
#if PA_BUILDFLAG(ENABLE_PKEYS)
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
                                        size_t offset = 0,
                                        bool read_only = false) {
  WriteProtectThreadIsolatedMemory(thread_isolation, (char*)&var + offset,
                                   sizeof(T) - offset, read_only);
}

int MprotectWithThreadIsolation(void* addr,
                                size_t len,
                                int prot,
                                ThreadIsolationOption thread_isolation) {
#if PA_BUILDFLAG(ENABLE_PKEYS)
  return PkeyMprotect(addr, len, prot, thread_isolation.pkey);
#endif
}

void WriteProtectThreadIsolatedGlobals(ThreadIsolationOption thread_isolation) {
  WriteProtectThreadIsolatedVariable(thread_isolation,
                                     PartitionAddressSpace::setup_, 0, true);

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

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  WriteProtectThreadIsolatedVariable(thread_isolation,
                                     ThreadIsolationSettings::settings);
#endif
}

void UnprotectThreadIsolatedGlobals() {
  WriteProtectThreadIsolatedGlobals(ThreadIsolationOption(false));
}

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
