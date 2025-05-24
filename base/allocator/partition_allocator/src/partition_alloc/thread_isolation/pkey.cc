// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/thread_isolation/pkey.h"

#if PA_BUILDFLAG(ENABLE_PKEYS)

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>

#include "partition_alloc/partition_alloc_base/cpu.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

#if !PA_BUILDFLAG(IS_LINUX) && !PA_BUILDFLAG(IS_CHROMEOS)
#error "This pkey code is currently only supported on Linux and ChromeOS"
#endif

namespace partition_alloc::internal {

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
bool CPUHasPkeySupport() {
  return base::CPU::GetInstanceNoAllocation().has_pku();
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
int PkeyMprotect(void* addr, size_t len, int prot, int pkey) {
  return syscall(SYS_pkey_mprotect, addr, len, prot, pkey);
}

void TagMemoryWithPkey(int pkey, void* address, size_t size) {
  PA_DCHECK((reinterpret_cast<uintptr_t>(address) &
             PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) == 0);
  PA_PCHECK(PkeyMprotect(address,
                         (size + PA_THREAD_ISOLATED_ALIGN_OFFSET_MASK) &
                             PA_THREAD_ISOLATED_ALIGN_BASE_MASK,
                         PROT_READ | PROT_WRITE, pkey) == 0);
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

#if PA_BUILDFLAG(DCHECKS_ARE_ON) || \
    PA_BUILDFLAG(ENABLE_PARTITION_LOCK_REENTRANCY_CHECK)

LiftPkeyRestrictionsScope::LiftPkeyRestrictionsScope()
    : saved_pkey_value_(kDefaultPkeyValue) {
  if (!ThreadIsolationSettings::settings.enabled) {
    return;
  }
  saved_pkey_value_ = Rdpkru();
  if (saved_pkey_value_ != kDefaultPkeyValue) {
    Wrpkru(kAllowAllPkeyValue);
  }
}

LiftPkeyRestrictionsScope::~LiftPkeyRestrictionsScope() {
  if (!ThreadIsolationSettings::settings.enabled) {
    return;
  }
  if (Rdpkru() != saved_pkey_value_) {
    Wrpkru(saved_pkey_value_);
  }
}

#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON) ||
        // PA_BUILDFLAG(ENABLE_PARTITION_LOCK_REENTRANCY_CHECK)

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(ENABLE_PKEYS)
