// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/process/memory.h"

#if defined(OS_WIN)
#include <windows.h>
#else
#include <unistd.h>
#endif  // defined(OS_WIN)

#include <string.h>

#include "base/allocator/buildflags.h"
#include "base/cxx17_backports.h"
#include "base/debug/alias.h"
#include "base/immediate_crash.h"
#include "base/logging.h"
#if BUILDFLAG(USE_PARTITION_ALLOC)
#include "base/allocator/partition_allocator/page_allocator.h"
#endif
#include "build/build_config.h"

namespace base {

size_t g_oom_size = 0U;

namespace internal {

// Crash server classifies base::internal::OnNoMemoryInternal as OOM.
NOINLINE void OnNoMemoryInternal(size_t size) {
  g_oom_size = size;
#if defined(OS_WIN)
  // Kill the process. This is important for security since most of code
  // does not check the result of memory allocation.
  // https://msdn.microsoft.com/en-us/library/het71c37.aspx
  // Pass the size of the failed request in an exception argument.
  ULONG_PTR exception_args[] = {size};
  ::RaiseException(base::win::kOomExceptionCode, EXCEPTION_NONCONTINUABLE,
                   base::size(exception_args), exception_args);

  // Safety check, make sure process exits here.
  _exit(win::kOomExceptionCode);
#else
  size_t tmp_size = size;
  base::debug::Alias(&tmp_size);

  // Note: Don't add anything that may allocate here. Depending on the
  // allocator, this may be called from within the allocator (e.g. with
  // PartitionAlloc), and would deadlock as our locks are not recursive.
  //
  // Additionally, this is unlikely to work, since allocating from an OOM
  // handler is likely to fail.
  //
  // Use IMMEDIATE_CRASH() so that the top frame in the crash is our code,
  // rather than using abort() or similar; this avoids the crash server needing
  // to be able to successfully unwind through libc to get to the correct
  // address, which is particularly an issue on Android.
  IMMEDIATE_CRASH();
#endif  // defined(OS_WIN)
}

}  // namespace internal

void TerminateBecauseOutOfMemory(size_t size) {
  internal::OnNoMemoryInternal(size);
}

// Defined in memory_mac.mm for macOS + use_allocator="none".  In case of
// USE_PARTITION_ALLOC_AS_MALLOC, no need to route the call to the system
// default calloc of macOS.
#if !defined(OS_APPLE) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

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

#endif  // !defined(OS_APPLE) || BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

namespace internal {
bool ReleaseAddressSpaceReservation() {
#if BUILDFLAG(USE_PARTITION_ALLOC)
  return ReleaseReservation();
#else
  return false;
#endif
}
}  // namespace internal

}  // namespace base
