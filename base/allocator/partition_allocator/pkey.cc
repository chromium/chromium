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

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool CPUHasPkeySupport() {
  return base::CPU::GetInstanceNoAllocation().has_pku();
}

PkeySettings PkeySettings::settings PA_PKEY_ALIGN;

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
int PkeyMprotect(void* addr, size_t len, int prot, int pkey) {
  return syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
}

int PkeyMprotectIfEnabled(void* addr, size_t len, int prot, int pkey) {
  if (PA_UNLIKELY(PkeySettings::settings.enabled)) {
    return PkeyMprotect(addr, len, prot, pkey);
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

  TagVariableWithPkey(pkey, PkeySettings::settings);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
int PkeyAlloc(int access_rights) {
  return syscall(SYS_pkey_alloc, 0, access_rights);
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PkeyFree(int pkey) {
  PA_PCHECK(syscall(SYS_pkey_free, pkey) == 0);
}

uint32_t Rdpkru() {
  uint32_t pkru;
  asm volatile(".byte 0x0f,0x01,0xee\n" : "=a"(pkru) : "c"(0), "d"(0));
  return pkru;
}

void Wrpkru(uint32_t pkru) {
  asm volatile(".byte 0x0f,0x01,0xef\n" : : "a"(pkru), "c"(0), "d"(0));
}

#if BUILDFLAG(PA_DCHECK_IS_ON)

LiftPkeyRestrictionsScope::LiftPkeyRestrictionsScope()
    : saved_pkey_value_(kDefaultPkeyValue) {
  if (!PkeySettings::settings.enabled) {
    return;
  }
  saved_pkey_value_ = Rdpkru();
  if (saved_pkey_value_ != kDefaultPkeyValue) {
    Wrpkru(kAllowAllPkeyValue);
  }
}

LiftPkeyRestrictionsScope::~LiftPkeyRestrictionsScope() {
  if (!PkeySettings::settings.enabled) {
    return;
  }
  if (Rdpkru() != saved_pkey_value_) {
    Wrpkru(saved_pkey_value_);
  }
}

#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_PKEYS)
