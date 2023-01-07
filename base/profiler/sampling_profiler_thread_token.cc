// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sampling_profiler_thread_token.h"

#include "build/build_config.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#include <pthread.h>

#include "base/profiler/stack_base_address_posix.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#endif

namespace base {

SamplingProfilerThreadToken GetSamplingProfilerCurrentThreadToken() {
  PlatformThreadId id = PlatformThread::CurrentId();
#if BUILDFLAG(IS_ANDROID)
  return {id, pthread_self()};
#elif BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  absl::optional<uintptr_t> maybe_stack_base =
      GetThreadStackBaseAddress(id, pthread_self());
  // GetThreadStackBaseAddress should only return nullopt on Android, so
  // dereferencing should be safe.
  CHECK(maybe_stack_base);
  return {id, *maybe_stack_base};
#else
  return {id};
#endif
}

}  // namespace base
