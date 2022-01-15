// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/sampling_profiler_thread_token.h"

#include "build/build_config.h"

namespace base {

SamplingProfilerThreadToken GetSamplingProfilerCurrentThreadToken() {
#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
  return {PlatformThread::CurrentId(), pthread_self()};
#else
  return {PlatformThread::CurrentId()};
#endif
}

}  // namespace base
