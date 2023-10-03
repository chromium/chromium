// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_PKEY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_PKEY_H_

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_PKEYS)

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/debugging_buildflags.h"
#include "base/allocator/partition_allocator/src/partition_alloc/thread_isolation/alignment.h"

#include <cstddef>
#include <cstdint>

namespace partition_alloc::internal {

constexpr int kDefaultPkey = 0;
constexpr int kInvalidPkey = -1;

// Check if the CPU supports pkeys.
bool CPUHasPkeySupport();

// A wrapper around the pkey_mprotect syscall.
[[nodiscard]] int PkeyMprotect(void* addr, size_t len, int prot, int pkey);

void TagMemoryWithPkey(int pkey, void* address, size_t size);

int PkeyAlloc(int access_rights);

void PkeyFree(int pkey);

// Read the pkru register (the current pkey state).
uint32_t Rdpkru();

// Write the pkru register (the current pkey state).
void Wrpkru(uint32_t pkru);

#if BUILDFLAG(PA_DCHECK_IS_ON)

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) LiftPkeyRestrictionsScope {
 public:
  static constexpr uint32_t kDefaultPkeyValue = 0x55555554;
  static constexpr uint32_t kAllowAllPkeyValue = 0x0;

  LiftPkeyRestrictionsScope();
  ~LiftPkeyRestrictionsScope();

 private:
  uint32_t saved_pkey_value_;
};

#endif  // BUILDFLAG(PA_DCHECK_IS_ON)

}  // namespace partition_alloc::internal

#endif  // BUILDFLAG(ENABLE_PKEYS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_THREAD_ISOLATION_PKEY_H_
