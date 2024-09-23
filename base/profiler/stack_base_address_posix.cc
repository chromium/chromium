// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_base_address_posix.h"

#include "base/check_op.h"
#include "base/logging.h"
#include "base/process/process_handle.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_ANDROID)
#include <inttypes.h>
#include <stdio.h>

#include "base/files/file_util.h"
#include "base/files/scoped_file.h"
#endif

#if BUILDFLAG(IS_CHROMEOS)
extern "C" void* __libc_stack_end;
#endif

namespace base {

namespace {

#if BUILDFLAG(IS_ANDROID)
std::optional<uintptr_t> GetAndroidMainThreadStackBaseAddressImpl() {
  char line[1024];
  base::ScopedFILE fp(base::OpenFile(base::FilePath("/proc/self/maps"), "r"));
  uintptr_t stack_addr = reinterpret_cast<uintptr_t>(line);
  if (!fp)
    return std::nullopt;
  while (fgets(line, sizeof(line), fp.get()) != nullptr) {
    uintptr_t start, end;
    if (sscanf(line, "%" SCNxPTR "-%" SCNxPTR, &start, &end) == 2) {
      if (start <= stack_addr && stack_addr < end)
        return end;
    }
  }
  return std::nullopt;
}
#endif

#if !BUILDFLAG(IS_LINUX)
uintptr_t GetThreadStackBaseAddressImpl(pthread_t pthread_id) {
  pthread_attr_t attr;
  // pthread_getattr_np will crash on ChromeOS & Linux if we are in the sandbox
  // and pthread_id refers to a different thread, due to the use of
  // sched_getaffinity().
  int result = pthread_getattr_np(pthread_id, &attr);
  // pthread_getattr_np should never fail except on Linux, and Linux will never
  // call this function. See
  // https://crrev.com/c/chromium/src/+/4064700/comment/ef75d2b7_c255168c/ for
  // discussion of crashing vs. returning nullopt in this case.
  CHECK_EQ(result, 0) << "pthread_getattr_np returned "
                      << logging::SystemErrorCodeToString(result);
  // See crbug.com/617730 for limitations of this approach on Linux-like
  // systems.
  void* address;
  size_t size;
  result = pthread_attr_getstack(&attr, &address, &size);
  CHECK_EQ(result, 0) << "pthread_attr_getstack returned "
                      << logging::SystemErrorCodeToString(result);
  pthread_attr_destroy(&attr);
  const uintptr_t base_address = reinterpret_cast<uintptr_t>(address) + size;
  return base_address;
}
#endif  // !BUILDFLAG(IS_LINUX)

}  // namespace

std::optional<uintptr_t> GetThreadStackBaseAddress(PlatformThreadId id,
                                                   pthread_t pthread_id) {
#if BUILDFLAG(IS_LINUX)
  // We don't currently support Linux; pthread_getattr_np() fails for the main
  // thread after zygote forks. https://crbug.com/1394278. Since we don't
  // support stack profiling at all on Linux, we just return nullopt instead of
  // trying to work around the problem.
  return std::nullopt;
#else
  const bool is_main_thread = id == GetCurrentProcId();
  if (is_main_thread) {
#if BUILDFLAG(IS_ANDROID)
    // The implementation of pthread_getattr_np() in Bionic reads proc/self/maps
    // to find the main thread base address, and throws SIGABRT when it fails to
    // read or parse the file. So, try to read the maps to get the main thread
    // stack base and cache the result. Other thread base addresses are sourced
    // from pthread state so are cheap to get.
    static const std::optional<uintptr_t> main_thread_base_address =
        GetAndroidMainThreadStackBaseAddressImpl();
    return main_thread_base_address;
#elif BUILDFLAG(IS_CHROMEOS)
    // Similarly, the sandbox will prevent pthread_getattr_np() from working
    // on the main thread in ChromeOS. Here, we have a simpler solution.
    return reinterpret_cast<uintptr_t>(__libc_stack_end);
#endif
  }
  return GetThreadStackBaseAddressImpl(pthread_id);
#endif  // !BUILDFLAG(IS_LINUX)
}

}  // namespace base
