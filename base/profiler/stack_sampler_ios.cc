// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include "base/memory/ptr_util.h"
#include "base/profiler/profiler_buildflags.h"
#include "build/build_config.h"

#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/suspendable_thread_delegate_mac.h"
#endif

namespace base {

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    std::unique_ptr<StackUnwindData> stack_unwind_data,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  return base::WrapUnique(new StackSampler(
      std::make_unique<StackCopierSuspend>(
          std::make_unique<SuspendableThreadDelegateMac>(thread_token)),
      std::move(stack_unwind_data), std::move(core_unwinders_factory),
      std::move(record_sample_callback), test_delegate));
#else
  return nullptr;
#endif
}

// static
size_t StackSampler::GetStackBufferSize() {
#if BUILDFLAG(IOS_STACK_PROFILER_ENABLED)
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default iOS main thread stack size
  // of 1 MB with extra wiggle room.
  return stack_size > 0 ? stack_size : 1536 * 1024;
#else
  return 0;
#endif
}

}  // namespace base
