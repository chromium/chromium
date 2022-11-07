// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/pkey.h"
#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"

#if BUILDFLAG(ENABLE_PKEYS)

#include <errno.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include "base/allocator/partition_allocator/partition_alloc_base/cpu.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"

#if !BUILDFLAG(IS_LINUX)
#error "This pkey code is currently only supported on Linux"
#endif

namespace partition_alloc::internal {

bool CPUHasPkeySupport() {
  return base::CPU::GetInstanceNoAllocation().has_pku();
}

int PkeyMprotect(void* addr, size_t len, int prot, int pkey) {
  if (PA_UNLIKELY(CPUHasPkeySupport())) {
    // The pkey_mprotect syscall is supported from Linux 4.9. If the CPU is
    // recent enough to have PKU support, then it's likely that we also run on a
    // more recent kernel. But fall back to mprotect if the syscall is not
    // available and pkey is 0.
    // Note that we can't use mprotect as the default for the pkey == 0 case,
    // since we can temporarily change the pkey back to 0 on some globals.
    int ret = syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
    if (PA_LIKELY(ret == 0))
      return ret;
    if (errno != ENOSYS)
      return ret;
    // fall through if syscall doesn't exist
  }
  PA_CHECK(pkey == 0);

  return mprotect(addr, len, prot);
}

void TagMemoryWithPkey(int pkey, void* address, size_t size) {
  PA_DCHECK(
      (reinterpret_cast<uintptr_t>(address) & PA_PKEY_ALIGN_OFFSET_MASK) == 0);
  PA_PCHECK(
      PkeyMprotect(address,
                   (size + PA_PKEY_ALIGN_OFFSET_MASK) & PA_PKEY_ALIGN_BASE_MASK,
                   PROT_READ | PROT_WRITE, pkey) == 0);
}

template <typename T>
void TagVariableWithPkey(int pkey, T& var) {
  TagMemoryWithPkey(pkey, &var, sizeof(T));
}

void TagGlobalsWithPkey(int pkey) {
  TagVariableWithPkey(pkey, PartitionAddressSpace::setup_);

  AddressPoolManager::Pool* pool =
      AddressPoolManager::GetInstance().GetPool(kPkeyPoolHandle);
  TagVariableWithPkey(pkey, *pool);

  uint16_t* pkey_reservation_offset_table =
      GetReservationOffsetTable(kPkeyPoolHandle);
  TagMemoryWithPkey(pkey, pkey_reservation_offset_table,
                    ReservationOffsetTable::kReservationOffsetTableLength);
}

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_PKEYS)
