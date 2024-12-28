// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <pthread.h>

#include <memory>

#include "base/memory/ptr_util.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_CHROMEOS) && \
    (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#endif

namespace base {

std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    std::unique_ptr<StackUnwindData> stack_unwind_data,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
#if BUILDFLAG(IS_CHROMEOS) && \
    (defined(ARCH_CPU_X86_64) || defined(ARCH_CPU_ARM64))
  return base::WrapUnique(new StackSampler(
      std::make_unique<StackCopierSignal>(
          ThreadDelegatePosix::Create(thread_token)),
      std::move(stack_unwind_data), std::move(core_unwinders_factory),
      std::move(record_sample_callback), test_delegate));
#else
  return nullptr;
#endif
}

size_t StackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  pthread_attr_t attr;
  if (stack_size == 0 && pthread_attr_init(&attr) == 0) {
    if (pthread_attr_getstacksize(&attr, &stack_size) != 0) {
      stack_size = 0;
    }
    pthread_attr_destroy(&attr);
  }

  // Maximum limits under NPTL implementation.
  constexpr size_t kDefaultStackLimit = 4 * (1 << 20);
  return stack_size > 0 ? stack_size : kDefaultStackLimit;
}

}  // namespace base
