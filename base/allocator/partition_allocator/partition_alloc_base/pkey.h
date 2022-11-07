// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_PKEY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_PKEY_H_

#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

#if BUILDFLAG(ENABLE_PKEYS)

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"

#include <cstddef>
#include <cstdint>

#if !defined(PA_HAS_64_BITS_POINTERS)
#error "pkey support requires 64 bit pointers"
#endif

namespace partition_alloc::internal::base {

constexpr int kDefaultPkey = 0;
constexpr int kInvalidPkey = -1;

// Check if the CPU supports pkeys.
bool CPUHasPkeySupport();

// A wrapper around pkey_mprotect that falls back to regular mprotect if the
// CPU/kernel doesn't support it (and pkey is 0).
[[nodiscard]] int PkeyMprotect(void* addr, size_t len, int prot, int pkey);

}  // namespace partition_alloc::internal::base

#endif  // BUILDFLAG(ENABLE_PKEYS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_PKEY_H_
