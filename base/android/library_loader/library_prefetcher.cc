// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/android/library_loader/library_prefetcher.h"

#include <sys/types.h>
#include <sys/wait.h>

#include <csignal>
#include <cstddef>

#include "base/android/library_loader/anchor_functions.h"
#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/bits.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/time/time.h"
#include "build/build_config.h"

#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
#include "base/android/orderfile/orderfile_instrumentation.h"  // nogncheck
#endif

#if BUILDFLAG(SUPPORTS_CODE_ORDERING)

namespace base {
namespace android {

namespace {

#if !BUILDFLAG(ORDERFILE_INSTRUMENTATION)
// The binary is aligned to a minimum 16K page size on AArch64 Android, else 4K.
#if defined(ARCH_CPU_ARM64)
constexpr size_t kPageSize = 16384;
#else
constexpr size_t kPageSize = 4096;
#endif
// Reads a byte per page between |start| and |end| to force it into the page
// cache.
// Heap allocations, syscalls and library functions are not allowed in this
// function.
// Returns true for success.
#if defined(ADDRESS_SANITIZER)
// Disable AddressSanitizer instrumentation for this function. It is touching
// memory that hasn't been allocated by the app, though the addresses are
// valid. Furthermore, this takes place in a child process. See crbug.com/653372
// for the context.
__attribute__((no_sanitize_address))
#endif
void Prefetch(size_t start, size_t end) {
  unsigned char* start_ptr = reinterpret_cast<unsigned char*>(start);
  unsigned char* end_ptr = reinterpret_cast<unsigned char*>(end);
  [[maybe_unused]] unsigned char dummy = 0;
  for (unsigned char* ptr = start_ptr; ptr < end_ptr; ptr += kPageSize) {
    // Volatile is required to prevent the compiler from eliminating this
    // loop.
    dummy ^= *static_cast<volatile unsigned char*>(ptr);
  }
}

// These values were used in the past for recording
// "LibraryLoader.PrefetchDetailedStatus".
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. See PrefetchStatus in enums.xml.
enum class PrefetchStatus {
  kSuccess = 0,
  kWrongOrdering = 1,
  kForkFailed = 2,
  kChildProcessCrashed = 3,
  kChildProcessKilled = 4,
  kMaxValue = kChildProcessKilled
};

PrefetchStatus ForkAndPrefetch() {
  if (!IsOrderingSane()) {
    LOG(WARNING) << "Incorrect code ordering";
    return PrefetchStatus::kWrongOrdering;
  }

  pid_t pid = fork();
  if (pid == 0) {
    // Android defines the background priority to this value since at least 2009
    // (see Process.java).
    constexpr int kBackgroundPriority = 10;
    setpriority(PRIO_PROCESS, 0, kBackgroundPriority);

    // |kStartOfText| may not be at the beginning of a page, since .plt can be
    // before it, yet in the same mapping for instance.
    size_t text_start_page = kStartOfText - kStartOfText % kPageSize;
    // Set the end to the page on which the beginning of the last symbol is. The
    // actual symbol may spill into the next page by a few bytes, but this is
    // outside of the executable code range anyway.
    size_t text_end_page = bits::AlignUp(kEndOfText, kPageSize);

    size_t ordered_start_page =
        kStartOfOrderedText - kStartOfOrderedText % kPageSize;
    // kEndOfUnorderedText is not considered ordered, but the byte immediately
    // before is considered ordered and so can not be contained in the start
    // page.
    size_t ordered_end_page = bits::AlignUp(kEndOfOrderedText, kPageSize);

    // Fetch the ordered section first.
    Prefetch(ordered_start_page, ordered_end_page);
    Prefetch(text_start_page, text_end_page);

    // _exit() doesn't call the atexit() handlers.
    _exit(EXIT_SUCCESS);
  } else {
    if (pid < 0) {
      return PrefetchStatus::kForkFailed;
    }
    int status;
    const pid_t result = HANDLE_EINTR(waitpid(pid, &status, 0));
    if (result == pid) {
      if (WIFEXITED(status)) {
        return PrefetchStatus::kSuccess;
      }
      if (WIFSIGNALED(status)) {
        int signal = WTERMSIG(status);
        switch (signal) {
          case SIGSEGV:
          case SIGBUS:
            return PrefetchStatus::kChildProcessCrashed;
          case SIGKILL:
          case SIGTERM:
          default:
            return PrefetchStatus::kChildProcessKilled;
        }
      }
    }
    // Should not happen. Per man waitpid(2), errors are:
    // - EINTR: handled.
    // - ECHILD if the process doesn't have an unwaited-for child with this PID.
    // - EINVAL.
    return PrefetchStatus::kChildProcessKilled;
  }
}
#endif  // !BUILDFLAG(ORDERFILE_INSTRUMENTATION)

}  // namespace

// static
void NativeLibraryPrefetcher::ForkAndPrefetchNativeLibrary() {
#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  // Avoid forking with orderfile instrumentation because the child process
  // would create a dump as well.
  return;
#else
  base::TimeTicks start_time = base::TimeTicks::Now();
  PrefetchStatus status = ForkAndPrefetch();
  base::UmaHistogramMediumTimes("Android.LibraryLoader.Prefetch.Duration",
                                base::TimeTicks::Now() - start_time);
  base::UmaHistogramEnumeration("Android.LibraryLoader.Prefetch.Status",
                                status);
  if (status != PrefetchStatus::kSuccess) {
    LOG(WARNING) << "Cannot prefetch the library. status = "
                 << static_cast<int>(status);
  }
#endif  // BUILDFLAG(ORDERFILE_INSTRUMENTATION)
}

}  // namespace android
}  // namespace base
#endif  // BUILDFLAG(SUPPORTS_CODE_ORDERING)
