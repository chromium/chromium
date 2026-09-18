// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sampling_profiler_thread_token.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>

#include "base/profiler/stack_base_address_posix.h"
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
#else
  const auto& [target_id] = *this;
  return {target_id};
#endif
}

SamplingProfilerThreadToken GetSamplingProfilerCurrentThreadToken() {
  PlatformThreadId id = PlatformThread::CurrentId();
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_APPLE)
  return {id, pthread_self()};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  std::optional<uintptr_t> maybe_stack_base =
      GetThreadStackBaseAddress(id, pthread_self());
  return {id, maybe_stack_base};
#else
  return {id};
#endif
}

}  // namespace base
