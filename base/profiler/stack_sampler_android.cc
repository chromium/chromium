// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <pthread.h>

#include "base/check.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/stack_copier_signal.h"
#include "base/profiler/thread_delegate_posix.h"
#include "base/profiler/unwinder.h"
#include "base/threading/platform_thread.h"

namespace base {

std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    std::unique_ptr<StackUnwindData> stack_unwind_data,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  auto thread_delegate = ThreadDelegatePosix::Create(thread_token);
  if (!thread_delegate)
    return nullptr;
  return base::WrapUnique(new StackSampler(
      std::make_unique<StackCopierSignal>(std::move(thread_delegate)),
      std::move(stack_unwind_data), std::move(core_unwinders_factory),
      std::move(record_sample_callback), test_delegate));
}

size_t StackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  pthread_attr_t attr;
  if (stack_size == 0 && pthread_attr_init(&attr) == 0) {
    if (pthread_attr_getstacksize(&attr, &stack_size) != 0)
      stack_size = 0;
    pthread_attr_destroy(&attr);
  }

  // 1MB is default thread limit set by Android at art/runtime/thread_pool.h.
  constexpr size_t kDefaultStackLimit = 1 << 20;
  return stack_size > 0 ? stack_size : kDefaultStackLimit;
}

}  // namespace base
