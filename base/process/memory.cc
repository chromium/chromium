// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/page_allocator.h"
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif  // BUILDFLAG(IS_WIN)

namespace base {

// Defined in memory_mac.mm for macOS + use_partition_alloc_as_malloc=false.
// In case of use_partition_alloc_as_malloc=true, no need to route the call to
// the system default calloc of macOS.
#if !BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

bool UncheckedCalloc(size_t num_items, size_t size, void** result) {
  const size_t alloc_size = num_items * size;

  // Overflow check
  if (size && ((alloc_size / size) != num_items)) {
    *result = nullptr;
    return false;
  }

  if (!UncheckedMalloc(alloc_size, result))
    return false;

  memset(*result, 0, alloc_size);
  return true;
}

#endif  // !BUILDFLAG(IS_APPLE) || PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace internal {
bool ReleaseAddressSpaceReservation() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  return partition_alloc::ReleaseReservation();
#else
  return false;
#endif
}
}  // namespace internal

}  // namespace base
