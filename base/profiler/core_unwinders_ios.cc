// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/core_unwinders.h"

#include <memory>
#include <vector>

#include "base/functional/bind.h"
#include "base/profiler/profiler_buildflags.h"

#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
#include "base/profiler/frame_pointer_unwinder.h"
#endif

namespace base {

StackSamplingProfiler::UnwindersFactory CreateCoreUnwindersFactory() {
#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  return BindOnce([] {
    std::vector<std::unique_ptr<Unwinder>> unwinders;
    if (__builtin_available(iOS 12.0, *)) {
      unwinders.push_back(std::make_unique<FramePointerUnwinder>());
    }
    return unwinders;
  });
#else   // BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  return StackSamplingProfiler::UnwindersFactory();
#endif  // BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
}

}  // namespace base
