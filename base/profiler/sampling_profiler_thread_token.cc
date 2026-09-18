// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sampling_profiler_thread_token.h"

#include "base/check_op.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>

#include "base/profiler/stack_base_address_posix.h"
#elif BUILDFLAG(IS_WIN)
#include <windows.h>

#include <processthreadsapi.h>

#include "base/check.h"
#include "base/win/scoped_handle.h"
#endif

namespace base {

SamplingProfilerThreadToken SamplingProfilerThreadToken::Clone() const {
  // Deconstruct `*this` using structured bindings to guarantee at compile
  // time that every member is accounted for when cloning. If a member is
  // added or removed from SamplingProfilerThreadToken on this platform,
  // the number of bindings will mismatch and fail compilation here.
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
  const auto& [target_id, target_pthread_id] = *this;
  return {target_id, target_pthread_id};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  const auto& [target_id, target_stack_base_address] = *this;
  return {target_id, target_stack_base_address};
#elif BUILDFLAG(IS_WIN)
  const auto& [target_id, target_thread_handle, target_stack_base_address] =
      *this;
  HANDLE duplicate = nullptr;
  if (target_thread_handle.is_valid()) {
    CHECK(::DuplicateHandle(::GetCurrentProcess(), target_thread_handle.get(),
                            ::GetCurrentProcess(), &duplicate, 0, FALSE,
                            DUPLICATE_SAME_ACCESS));
  }
  return {target_id, win::ScopedHandle(duplicate), target_stack_base_address};
#else
  const auto& [target_id] = *this;
  return {target_id};
#endif
}

SamplingProfilerThreadToken GetSamplingProfilerCurrentThreadToken() {
  PlatformThreadId id = PlatformThread::CurrentId();
  DCHECK_NE(id, kInvalidThreadId);
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
  return {id, pthread_self()};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::optional<uintptr_t> maybe_stack_base =
      GetThreadStackBaseAddress(id, pthread_self());
  return {id, maybe_stack_base};
#elif BUILDFLAG(IS_WIN)
  // Duplicate the current thread's pseudo-handle while on the thread itself.
  // This produces a real thread handle with THREAD_SUSPEND_RESUME and
  // THREAD_GET_CONTEXT access rights, avoiding later cross-thread
  // OpenThread() calls that may fail due to sandbox or security software
  // restrictions.
  HANDLE thread = nullptr;
  CHECK(::DuplicateHandle(::GetCurrentProcess(), ::GetCurrentThread(),
                          ::GetCurrentProcess(), &thread, 0, FALSE,
                          DUPLICATE_SAME_ACCESS));
  // On Windows, stacks grow downwards from high addresses to low addresses.
  // Therefore, the base of the stack is its upper limit (`high_limit`):
  // https://learn.microsoft.com/en-us/windows/win32/api/processthreadsapi/nf-processthreadsapi-getcurrentthreadstacklimits
  ULONG_PTR low_limit = 0;
  ULONG_PTR high_limit = 0;
  ::GetCurrentThreadStackLimits(&low_limit, &high_limit);
  CHECK_GT(high_limit, 0u);
  return {id, win::ScopedHandle(thread), static_cast<uintptr_t>(high_limit)};
#else
  return {id};
#endif
}

}  // namespace base
