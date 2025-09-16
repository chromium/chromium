// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/android/library_loader/library_prefetcher.h"

#include <sys/mman.h>
#include <sys/types.h>
#include <sys/wait.h>

#include <algorithm>
#include <array>
#include <cerrno>
#include <csignal>
#include <cstddef>
#include <string>

#include "base/android/library_loader/anchor_functions.h"
#include "base/android/orderfile/orderfile_buildflags.h"
#include "base/bits.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/features.h"
#include "base/logging.h"
#include "base/memory/page_size.h"
#include "base/metrics/histogram_functions.h"
#include "base/posix/eintr_wrapper.h"
#include "base/threading/platform_thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
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
// This might not match base::GetPageSize(), the booted Kernel's page size.
#if defined(ARCH_CPU_ARM64)
constexpr size_t kBinaryPageSize = 16384;
#else
constexpr size_t kBinaryPageSize = 4096;
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
  // It's possible that using kBinaryPageSize instead of page_size (or some
  // other value) is a bit arbitrary for the read stepping here. In practice,
  // disk readahead is greater than either, so probably doesn't matter too much.
  for (unsigned char* ptr = start_ptr; ptr < end_ptr; ptr += kBinaryPageSize) {
    // Volatile is required to prevent the compiler from eliminating this
    // loop.
    dummy ^= *static_cast<volatile unsigned char*>(ptr);
  }
}

// Call madvise from |start| to |end| in chunks of |target_length| (rounded up
// to a whole page). |target_length| can be 0 to madvise the entire range in a
// single call. |start| must already be aligned to a page.
int MadviseRange(size_t start, size_t end, int advice, size_t target_length) {
  if (target_length == 0) {
    target_length = end - start;
  }
  target_length = bits::AlignUp(target_length, base::GetPageSize());
  for (size_t current = start; current < end; current += target_length) {
    // madvise will round `end - current` up to a page size if required.
    size_t length = std::min(target_length, end - current);
    if (madvise(reinterpret_cast<void*>(current), length, advice) != 0) {
      return errno;
    }
  }
  return 0;
}

struct Section {
  static Section CreateAligned(std::string_view name,
                               size_t start_anchor,
                               size_t end_anchor) {
    return Section{
        .name = name,
        .start = start_anchor - start_anchor % kBinaryPageSize,
        .end = bits::AlignUp(end_anchor, kBinaryPageSize),
    };
  }

  std::string_view name;
  size_t start;
  size_t end;
};

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
  kMadviseFailed = 5,
  kMaxValue = kMadviseFailed,
};

PrefetchStatus PrefetchWithMadvise(base::span<const Section> sections,
                                   int madvise_advice,
                                   size_t madvise_length) {
  TRACE_EVENT("startup", "LibraryPrefetcher::PrefetchWithMadvise");
  for (const auto& section : sections) {
    int result = MadviseRange(section.start, section.end, madvise_advice,
                              madvise_length);
    if (result != 0) {
      PLOG(WARNING) << "madvise failed for " << section.name << " section";
      return PrefetchStatus::kMadviseFailed;
    }
  }
  return PrefetchStatus::kSuccess;
}

PrefetchStatus PrefetchWithFork(base::span<const Section> sections) {
  TRACE_EVENT("startup", "LibraryPrefetcher::PrefetchWithFork");
  base::TimeTicks fork_start_time = base::TimeTicks::Now();
  pid_t pid = fork();
  if (pid == 0) {
    // Android defines the background priority to this value since at least 2009
    // (see Process.java).
    constexpr int kBackgroundPriority = 10;
    setpriority(PRIO_PROCESS, 0, kBackgroundPriority);
    for (const auto& section : sections) {
      Prefetch(section.start, section.end);
    }
    // _exit() doesn't call the atexit() handlers.
    _exit(EXIT_SUCCESS);
  } else {
    base::UmaHistogramCustomMicrosecondsTimes(
        "Android.LibraryLoader.Prefetch.ForkDuration",
        base::TimeTicks::Now() - fork_start_time, base::Microseconds(1),
        base::Seconds(1), 50);
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

PrefetchStatus PrefetchWithForkOrMadvise() {
  if (!IsOrderingSane()) {
    LOG(WARNING) << "Incorrect code ordering";
    return PrefetchStatus::kWrongOrdering;
  }

  const std::array<const Section, 2> sections{
      // Fetch the ordered section first.
      Section::CreateAligned("ordered", kStartOfOrderedText, kEndOfOrderedText),
      Section::CreateAligned("text", kStartOfText, kEndOfText),
  };

  if (base::FeatureList::IsEnabled(features::kLibraryPrefetcherMadvise)) {
    // MADV_POPULATE_READ was an alternative considered. The differences being:
    // - It can be used reliably on the entire range at once.
    // - It takes longer than multiple WILLNEEDs of moderate (~64K) length.
    // - It grows attributed RSS aggressively. (WILLNEED doesn't.)
    // - It is not as widely supported as WILLNEED.
    // - (At time of writing) it is not already allowlisted in our seccomp BPF.
    constexpr int madvise_advice = MADV_WILLNEED;

    // The man page for madvise(2) expressly supports using madvise(0, 0, ...)
    // as a way to probe support, but it still trips -Wnonnull.
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnonnull"
    bool supported = madvise(nullptr, 0, madvise_advice) == 0;
#pragma clang diagnostic pop
    if (!supported) {
      // PLOG immediately after check so that nothing overwrites errno.
      PLOG(WARNING) << "madvise not supported for library prefetch";
    }
    base::UmaHistogramBoolean("Android.LibraryLoader.Prefetch.Madvise",
                              supported);

    if (supported) {
      base::ThreadType old_thread_type =
          base::PlatformThread::GetCurrentThreadType();
      base::PlatformThread::SetCurrentThreadType(base::ThreadType::kBackground);
      PrefetchStatus status =
          PrefetchWithMadvise(sections, madvise_advice,
                              features::kLibraryPrefetcherMadviseLength.Get());
      base::PlatformThread::SetCurrentThreadType(old_thread_type);
      return status;
    }

    if (!features::kLibraryPrefetcherMadviseFallback.Get()) {
      return PrefetchStatus::kMadviseFailed;
    }
    LOG(WARNING) << "falling back to fork-based library prefetch";
  }

  return PrefetchWithFork(sections);
}
#endif  // !BUILDFLAG(ORDERFILE_INSTRUMENTATION)

}  // namespace

// static
void NativeLibraryPrefetcher::PrefetchNativeLibrary() {
#if BUILDFLAG(ORDERFILE_INSTRUMENTATION)
  // Avoid forking with orderfile instrumentation because the child process
  // would create a dump as well.
  return;
#else
  base::TimeTicks start_time = base::TimeTicks::Now();
  PrefetchStatus status = PrefetchWithForkOrMadvise();
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
