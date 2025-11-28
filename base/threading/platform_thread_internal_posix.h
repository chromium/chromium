// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
#define BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_

#include <optional>

#include "base/base_export.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace base {

namespace internal {

struct ThreadTypeToNiceValuePairForTest {
  ThreadType priority;
  int nice_value;
};

// The elements must be listed in the order of decreasing priority (highest
// priority first), that is, in the order of increasing nice values (lowest nice
// value first).
extern const ThreadTypeToNiceValuePairForTest
    kThreadTypeToNiceValueMapForTest[7];

// Returns the nice value matching |priority| based on the platform-specific
// implementation of kThreadTypeToNiceValueMap.
int ThreadTypeToNiceValue(ThreadType thread_type);

// Returns whether SetCurrentThreadType can set a thread as kRealtimeAudio.
bool CanSetThreadTypeToRealtimeAudio();

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// Current thread id is cached in thread local storage for performance reasons.
// In some rare cases it's important to invalidate that cache explicitly (e.g.
// after going through clone() syscall which does not call pthread_atfork()
// handlers).
// This can only be called when the process is single-threaded.
BASE_EXPORT void InvalidateTidCache();
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

// Returns the ThreadPrioirtyForTest matching |nice_value| based on the
// platform-specific implementation of kThreadTypeToNiceValueMapForTest.
ThreadType NiceValueToThreadTypeForTest(int nice_value);

std::optional<ThreadType> GetCurrentEffectiveThreadTypeForPlatformForTest();

int GetCurrentThreadNiceValue();
int GetThreadNiceValue(PlatformThreadId id);

bool SetThreadNiceFromType(PlatformThreadId thread_id, ThreadType thread_type);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
void SetThreadTypeLinux(ProcessId process_id,
                        PlatformThreadId thread_id,
                        ThreadType thread_type,
                        IsViaIPC via_ipc);
#endif
#if BUILDFLAG(IS_CHROMEOS)
void SetThreadTypeChromeOS(ProcessId process_id,
                           PlatformThreadId thread_id,
                           ThreadType thread_type,
                           IsViaIPC via_ipc);
#endif
#if BUILDFLAG(IS_CHROMEOS)
inline constexpr auto SetThreadType = SetThreadTypeChromeOS;
#elif BUILDFLAG(IS_LINUX)
inline constexpr auto SetThreadType = SetThreadTypeLinux;
#endif

}  // namespace internal

}  // namespace base

#endif  // BASE_THREADING_PLATFORM_THREAD_INTERNAL_POSIX_H_
