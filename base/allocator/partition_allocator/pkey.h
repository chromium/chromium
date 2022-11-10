// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PKEY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PKEY_H_

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_PKEYS)

#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"

#include <cstddef>
#include <cstdint>

#if !defined(PA_HAS_64_BITS_POINTERS)
#error "pkey support requires 64 bit pointers"
#endif

#define PA_PKEY_ALIGN_SZ SystemPageSize()
#define PA_PKEY_ALIGN_OFFSET_MASK (PA_PKEY_ALIGN_SZ - 1)
#define PA_PKEY_ALIGN_BASE_MASK (~PA_PKEY_ALIGN_OFFSET_MASK)
#define PA_PKEY_ALIGN alignas(PA_PKEY_ALIGN_SZ)

#define PA_PKEY_FILL_PAGE_SZ(size) \
  ((PA_PKEY_ALIGN_SZ - (size & PA_PKEY_ALIGN_OFFSET_MASK)) % PA_PKEY_ALIGN_SZ)
// Calculate the required padding so that the last element of a page-aligned
// array lands on a page boundary. In other words, calculate that padding so
// that (count-1) elements are a multiple of page size.
#define PA_PKEY_ARRAY_PAD_SZ(Type, count) \
  PA_PKEY_FILL_PAGE_SZ(sizeof(Type) * (count - 1))

namespace partition_alloc::internal {

constexpr int kDefaultPkey = 0;
constexpr int kInvalidPkey = -1;

// Check if the CPU supports pkeys.
bool CPUHasPkeySupport();

// A wrapper around pkey_mprotect that falls back to regular mprotect if
// PkeySettings::enabled is false.
[[nodiscard]] int PkeyMprotectIfEnabled(void* addr,
                                        size_t len,
                                        int prot,
                                        int pkey);
// A wrapper around pkey_mprotect without fallback.
[[nodiscard]] int PkeyMprotect(void* addr, size_t len, int prot, int pkey);

// If we set up a pkey pool, we need to tag global variables with the pkey to
// make them readable in case default pkey access is disabled. Called once
// during pkey pool initialization.
void TagGlobalsWithPkey(int pkey);

int PkeyAlloc(int access_rights);

void PkeyFree(int pkey);

// Read the pkru register (the current pkey state).
uint32_t Rdpkru();

// Write the pkru register (the current pkey state).
void Wrpkru(uint32_t pkru);

struct PkeySettings {
  bool enabled = false;
  char pad_[PA_PKEY_FILL_PAGE_SZ(sizeof(enabled))] = {};
  static PkeySettings settings PA_PKEY_ALIGN PA_CONSTINIT;
};

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

#else  // BUILDFLAG(ENABLE_PKEYS)
#define PA_PKEY_ALIGN
#define PA_PKEY_FILL_PAGE_SZ(size) 0
#define PA_PKEY_ARRAY_PAD_SZ(Type, size) 0
#endif  // BUILDFLAG(ENABLE_PKEYS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PKEY_H_
