// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/pkey.h"

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

namespace partition_alloc::internal::base {

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

}  // namespace partition_alloc::internal::base

#endif  // BUILDFLAG(ENABLE_PKEYS)
