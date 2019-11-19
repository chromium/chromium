// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler_impl.h"

#include <utility>

#include "base/logging.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

StackSamplerImpl::StackSamplerImpl(std::unique_ptr<StackCopier> stack_copier,
                                   std::unique_ptr<Unwinder> native_unwinder,
                                   ModuleCache* module_cache,
                                   StackSamplerTestDelegate* test_delegate)
    : stack_copier_(std::move(stack_copier)),
      native_unwinder_(std::move(native_unwinder)),
      module_cache_(module_cache),
      test_delegate_(test_delegate) {}

StackSamplerImpl::~StackSamplerImpl() = default;

void StackSamplerImpl::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  aux_unwinder_ = std::move(unwinder);
  aux_unwinder_->AddNonNativeModules(module_cache_);
}

void StackSamplerImpl::RecordStackFrames(StackBuffer* stack_buffer,
                                         ProfileBuilder* profile_builder) {
  DCHECK(stack_buffer);

  RegisterContext thread_context;
  uintptr_t stack_top;
  bool success = stack_copier_->CopyStack(stack_buffer, &stack_top,
                                          profile_builder, &thread_context);
  if (!success)
    return;

  if (test_delegate_)
    test_delegate_->OnPreStackWalk();

  profile_builder->OnSampleCompleted(
      WalkStack(module_cache_, &thread_context, stack_top,
                native_unwinder_.get(), aux_unwinder_.get()));
}
// static

std::vector<Frame> StackSamplerImpl::WalkStackForTesting(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    Unwinder* native_unwinder,
    Unwinder* aux_unwinder) {
  return WalkStack(module_cache, thread_context, stack_top, native_unwinder,
                   aux_unwinder);
}

// static
std::vector<Frame> StackSamplerImpl::WalkStack(ModuleCache* module_cache,
                                               RegisterContext* thread_context,
                                               uintptr_t stack_top,
                                               Unwinder* native_unwinder,
                                               Unwinder* aux_unwinder) {
  std::vector<Frame> stack;
  // Reserve enough memory for most stacks, to avoid repeated
  // allocations. Approximately 99.9% of recorded stacks are 128 frames or
  // fewer.
  stack.reserve(128);

  // Record the first frame from the context values.
  stack.emplace_back(RegisterContextInstructionPointer(thread_context),
                     module_cache->GetModuleForAddress(
                         RegisterContextInstructionPointer(thread_context)));

  size_t prior_stack_size;
  UnwindResult result;
  do {
    // Choose an authoritative unwinder for the current module. Use the aux
    // unwinder if it thinks it can unwind from the current frame, otherwise use
    // the native unwinder.
    Unwinder* unwinder =
        aux_unwinder && aux_unwinder->CanUnwindFrom(&stack.back())
            ? aux_unwinder
            : native_unwinder;

    prior_stack_size = stack.size();
    result =
        unwinder->TryUnwind(thread_context, stack_top, module_cache, &stack);

    // The native unwinder should be the only one that returns COMPLETED
    // since the stack starts in native code.
    DCHECK(result != UnwindResult::COMPLETED || unwinder == native_unwinder);
  } while (result != UnwindResult::ABORTED &&
           result != UnwindResult::COMPLETED &&
           // Give up if the authoritative unwinder for the module was unable to
           // unwind.
           stack.size() > prior_stack_size);

  return stack;
}

}  // namespace base
