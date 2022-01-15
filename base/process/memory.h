// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROCESS_MEMORY_H_
#define BASE_PROCESS_MEMORY_H_

#include <stddef.h>

#include "base/base_export.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

namespace base {

// Enables 'terminate on heap corruption' flag. Helps protect against heap
// overflow. Has no effect if the OS doesn't provide the necessary facility.
BASE_EXPORT void EnableTerminationOnHeapCorruption();

// Turns on process termination if memory runs out.
BASE_EXPORT void EnableTerminationOnOutOfMemory();

// Terminates process. Should be called only for out of memory errors.
// |size| is the size of the failed allocation, or 0 if not known.
// Crash reporting classifies such crashes as OOM.
// Must be allocation-safe.
BASE_EXPORT void TerminateBecauseOutOfMemory(size_t size);

// Records the size of the allocation that caused the current OOM crash, for
// consumption by Breakpad.
// TODO: this can be removed when Breakpad is no longer supported.
BASE_EXPORT extern size_t g_oom_size;

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID) || \
    BUILDFLAG(IS_AIX)
// The maximum allowed value for the OOM score.
const int kMaxOomScore = 1000;

// This adjusts /proc/<pid>/oom_score_adj so the Linux OOM killer will
// prefer to kill certain process types over others. The range for the
// adjustment is [-1000, 1000], with [0, 1000] being user accessible.
// If the Linux system doesn't support the newer oom_score_adj range
// of [0, 1000], then we revert to using the older oom_adj, and
// translate the given value into [0, 15].  Some aliasing of values
// may occur in that case, of course.
BASE_EXPORT bool AdjustOOMScore(ProcessId process, int score);
#endif

namespace internal {
// Returns true if address-space was released. Some configurations reserve part
// of the process address-space for special allocations (e.g. WASM).
bool ReleaseAddressSpaceReservation();
}  // namespace internal

#if BUILDFLAG(IS_WIN)
namespace win {

// Custom Windows exception code chosen to indicate an out of memory error.
// See https://msdn.microsoft.com/en-us/library/het71c37.aspx.
// "To make sure that you do not define a code that conflicts with an existing
// exception code" ... "The resulting error code should therefore have the
// highest four bits set to hexadecimal E."
// 0xe0000008 was chosen arbitrarily, as 0x00000008 is ERROR_NOT_ENOUGH_MEMORY.
const DWORD kOomExceptionCode = 0xe0000008;

}  // namespace win
#endif

// Special allocator functions for callers that want to check for OOM.
// These will not abort if the allocation fails even if
// EnableTerminationOnOutOfMemory has been called.
// This can be useful for huge and/or unpredictable size memory allocations.
// Please only use this if you really handle the case when the allocation
// fails. Doing otherwise would risk security.
// These functions may still crash on OOM when running under memory tools,
// specifically ASan and other sanitizers.
// Return value tells whether the allocation succeeded. If it fails |result| is
// set to NULL, otherwise it holds the memory address.
//
// Note: You *must* use UncheckedFree() to free() the memory allocated, not
// regular free(). This also means that this a pointer allocated below cannot be
// passed to realloc().
[[nodiscard]] BASE_EXPORT bool UncheckedMalloc(size_t size, void** result);
[[nodiscard]] BASE_EXPORT bool UncheckedCalloc(size_t num_items,
                                               size_t size,
                                               void** result);

// *Must* be used to free memory allocated with base::UncheckedMalloc() and
// base::UncheckedCalloc().
// TODO(crbug.com/1279371): Enforce it, when all callers are converted.
BASE_EXPORT void UncheckedFree(void* ptr);

}  // namespace base

#endif  // BASE_PROCESS_MEMORY_H_
