// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/oom.h"

#include "base/allocator/partition_allocator/src/partition_alloc/oom_callback.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/immediate_crash.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>

#include <stdlib.h>

#include <array>
#endif  // BUILDFLAG(IS_WIN)

namespace partition_alloc {

size_t g_oom_size = 0U;

namespace internal {

// Crash server classifies base::internal::OnNoMemoryInternal as OOM.
// TODO(crbug.com/1151236): Update to
// partition_alloc::internal::base::internal::OnNoMemoryInternal
PA_NOINLINE void OnNoMemoryInternal(size_t size) {
  g_oom_size = size;
#if BUILDFLAG(IS_WIN)
  // Kill the process. This is important for security since most of code
  // does not check the result of memory allocation.
  // https://msdn.microsoft.com/en-us/library/het71c37.aspx
  // Pass the size of the failed request in an exception argument.
  ULONG_PTR exception_args[] = {size};
  ::RaiseException(win::kOomExceptionCode, EXCEPTION_NONCONTINUABLE,
                   std::size(exception_args), exception_args);

  // Safety check, make sure process exits here.
  _exit(win::kOomExceptionCode);
#else
  size_t tmp_size = size;
  internal::base::debug::Alias(&tmp_size);

  // Note: Don't add anything that may allocate here. Depending on the
  // allocator, this may be called from within the allocator (e.g. with
  // PartitionAlloc), and would deadlock as our locks are not recursive.
  //
  // Additionally, this is unlikely to work, since allocating from an OOM
  // handler is likely to fail.
  //
  // Use PA_IMMEDIATE_CRASH() so that the top frame in the crash is our code,
  // rather than using abort() or similar; this avoids the crash server needing
  // to be able to successfully unwind through libc to get to the correct
  // address, which is particularly an issue on Android.
  PA_IMMEDIATE_CRASH();
#endif  // BUILDFLAG(IS_WIN)
}

}  // namespace internal

void TerminateBecauseOutOfMemory(size_t size) {
  internal::OnNoMemoryInternal(size);
}

namespace internal {

// The crash is generated in a PA_NOINLINE function so that we can classify the
// crash as an OOM solely by analyzing the stack trace. It is tagged as
// PA_NOT_TAIL_CALLED to ensure that its parent function stays on the stack.
[[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED void OnNoMemory(size_t size) {
  RunPartitionAllocOomCallback();
  TerminateBecauseOutOfMemory(size);
  PA_IMMEDIATE_CRASH();
}

}  // namespace internal

}  // namespace partition_alloc
