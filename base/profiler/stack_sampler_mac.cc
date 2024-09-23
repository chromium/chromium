// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/ptr_util.h"
#include "base/profiler/frame_pointer_unwinder.h"
#include "base/profiler/stack_copier_suspend.h"
#include "base/profiler/suspendable_thread_delegate_mac.h"
#include "base/profiler/unwinder.h"

namespace base {

namespace {

std::vector<std::unique_ptr<Unwinder>> CreateUnwinders() {
  std::vector<std::unique_ptr<Unwinder>> unwinders;
  unwinders.push_back(std::make_unique<FramePointerUnwinder>());
  return unwinders;
}

}  // namespace

// static
std::unique_ptr<StackSampler> StackSampler::Create(
    SamplingProfilerThreadToken thread_token,
    std::unique_ptr<StackUnwindData> stack_unwind_data,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  DCHECK(!core_unwinders_factory);
  return base::WrapUnique(new StackSampler(
      std::make_unique<StackCopierSuspend>(
          std::make_unique<SuspendableThreadDelegateMac>(thread_token)),
      std::move(stack_unwind_data), BindOnce(&CreateUnwinders),
      std::move(record_sample_callback), test_delegate));
}

// static
size_t StackSampler::GetStackBufferSize() {
  size_t stack_size = PlatformThread::GetDefaultThreadStackSize();

  // If getrlimit somehow fails, return the default macOS main thread stack size
  // of 8 MB (DFLSSIZ in <i386/vmparam.h>) with extra wiggle room.
  return stack_size > 0 ? stack_size : 12 * 1024 * 1024;
}

}  // namespace base
