// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/oom.h"

#include "partition_alloc/build_config.h"
#include "partition_alloc/oom_callback.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>

#include <array>
#include <cstdlib>
#include <limits>
#endif  // PA_BUILDFLAG(IS_WIN)

namespace partition_alloc {

size_t g_oom_size = 0U;

namespace internal {

// Crash server classifies base::internal::OnNoMemoryInternal as OOM.
// TODO(crbug.com/40158212): Update to
// partition_alloc::internal::base::internal::OnNoMemoryInternal
[[noreturn]] PA_NOINLINE PA_NOT_TAIL_CALLED void OnNoMemoryInternal(
    size_t size) {
  g_oom_size = size;
  size_t tmp_size = size;
  internal::base::debug::Alias(&tmp_size);

#if PA_BUILDFLAG(IS_WIN)
  // Create an exception vector with:
  // [0] the size of the allocation, in bytes
  // [1] "current committed memory limit for the system or the current process,
  //     whichever is smaller, in bytes"
  // [2] "maximum amount of memory the current process can commit, in bytes"
  //
  // Citations from
  // https://learn.microsoft.com/en-us/windows/win32/api/sysinfoapi/ns-sysinfoapi-memorystatusex
  //
  // System commit constraints (which may be different from the process commit
  // constraints) are in the stability_report.SystemMemoryState.WindowsMemory
  // proto attached to crash reports.
  //
  // Note: Both the process commit constraints in the exception vector and the
  // system commit constraints in the proto are collected *after* the OOM and
  // may therefore not reflect the state at the time of the OOM (e.g. another
  // process may have exited or the page file may have been resized).
  constexpr size_t kInvalid = std::numeric_limits<ULONG_PTR>::max();
  ULONG_PTR exception_args[] = {size, kInvalid, kInvalid};

  MEMORYSTATUSEX memory_status = {};
  memory_status.dwLength = sizeof(memory_status);
  if (::GlobalMemoryStatusEx(&memory_status) != 0) {
    exception_args[1] = memory_status.ullTotalPageFile;
    exception_args[2] = memory_status.ullAvailPageFile;
  }
  internal::base::debug::Alias(&memory_status);

  // Kill the process. This is important for security since most of code
  // does not check the result of memory allocation.
  // Documentation: https://msdn.microsoft.com/en-us/library/het71c37.aspx
  ::RaiseException(win::kOomExceptionCode, EXCEPTION_NONCONTINUABLE,
                   std::size(exception_args), exception_args);

  // Safety check, make sure process exits here.
  _exit(win::kOomExceptionCode);
#else
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
#endif  // PA_BUILDFLAG(IS_WIN)
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
