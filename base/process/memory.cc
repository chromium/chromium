// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/390223051): Remove C-library calls to fix the errors.
#pragma allow_unsafe_libc_calls
#endif

#include "base/process/memory.h"

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if PA_BUILDFLAG(USE_ALLOCATOR_SHIM)
#include "partition_alloc/shim/allocator_shim.h"  // nogncheck
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
#include "partition_alloc/page_allocator.h"  // nogncheck
#endif

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif  // BUILDFLAG(IS_WIN)

namespace base::internal {
bool ReleaseAddressSpaceReservation() {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
  return partition_alloc::ReleaseReservation();
#else
  return false;
#endif
}
}  // namespace base::internal
