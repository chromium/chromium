// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler_impl.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "base/ranges/algorithm.h"
#include "build/build_config.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

namespace {

// Notifies the unwinders about the stack capture, and records metadata, while
// the thread is suspended.
class StackCopierDelegate : public StackCopier::Delegate {
 public:
  StackCopierDelegate(
      const base::circular_deque<std::unique_ptr<Unwinder>>* unwinders,
      ProfileBuilder* profile_builder,
      MetadataRecorder::MetadataProvider* metadata_provider)
      : unwinders_(unwinders),
        profile_builder_(profile_builder),
        metadata_provider_(metadata_provider) {}

  StackCopierDelegate(const StackCopierDelegate&) = delete;
  StackCopierDelegate& operator=(const StackCopierDelegate&) = delete;

  // StackCopier::Delegate:
  // IMPORTANT NOTE: to avoid deadlock this function must not invoke any
  // non-reentrant code that is also invoked by the target thread. In
  // particular, it may not perform any heap allocation or deallocation,
  // including indirectly via use of DCHECK/CHECK or other logging statements.
  void OnStackCopy() override {
    for (const auto& unwinder : *unwinders_)
      unwinder->OnStackCapture();

    profile_builder_->RecordMetadata(*metadata_provider_);
  }

 private:
  const base::circular_deque<std::unique_ptr<Unwinder>>* unwinders_;
  ProfileBuilder* const profile_builder_;
  const MetadataRecorder::MetadataProvider* const metadata_provider_;
};

}  // namespace

StackSamplerImpl::StackSamplerImpl(std::unique_ptr<StackCopier> stack_copier,
                                   UnwindersFactory core_unwinders_factory,
                                   ModuleCache* module_cache,
                                   RepeatingClosure record_sample_callback,
                                   StackSamplerTestDelegate* test_delegate)
    : stack_copier_(std::move(stack_copier)),
      unwinders_factory_(std::move(core_unwinders_factory)),
      module_cache_(module_cache),
      record_sample_callback_(std::move(record_sample_callback)),
      test_delegate_(test_delegate) {
  DCHECK(unwinders_factory_);
}

StackSamplerImpl::~StackSamplerImpl() = default;

void StackSamplerImpl::Initialize() {
  std::vector<std::unique_ptr<Unwinder>> unwinders =
      std::move(unwinders_factory_).Run();

  // |unwinders| is iterated backward since |unwinders_factory_| generates
  // unwinders in increasing priority order. |unwinders_| is stored in
  // decreasing priority order for ease of use within the class.
  unwinders_.insert(unwinders_.end(),
                    std::make_move_iterator(unwinders.rbegin()),
                    std::make_move_iterator(unwinders.rend()));

  for (const auto& unwinder : unwinders_)
    unwinder->Initialize(module_cache_);

  was_initialized_ = true;
}

void StackSamplerImpl::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  // Initialize() invokes Initialize() on the unwinders that are present
  // at the time. If it hasn't occurred yet, we allow it to add the initial
  // modules, otherwise we do it here.
  if (was_initialized_)
    unwinder->Initialize(module_cache_);
  unwinders_.push_front(std::move(unwinder));
}

void StackSamplerImpl::RecordStackFrames(StackBuffer* stack_buffer,
                                         ProfileBuilder* profile_builder) {
  DCHECK(stack_buffer);

  if (record_sample_callback_)
    record_sample_callback_.Run();

  RegisterContext thread_context;
  uintptr_t stack_top;
  TimeTicks timestamp;

  bool copy_stack_succeeded;
  {
    // Make this scope as small as possible because |metadata_provider| is
    // holding a lock.
    MetadataRecorder::MetadataProvider metadata_provider(
        GetSampleMetadataRecorder());
    StackCopierDelegate delegate(&unwinders_, profile_builder,
                                 &metadata_provider);
    copy_stack_succeeded = stack_copier_->CopyStack(
        stack_buffer, &stack_top, &timestamp, &thread_context, &delegate);
  }
  if (!copy_stack_succeeded) {
    profile_builder->OnSampleCompleted(
        {}, timestamp.is_null() ? TimeTicks::Now() : timestamp);
    return;
  }

  for (const auto& unwinder : unwinders_)
    unwinder->UpdateModules();

  if (test_delegate_)
    test_delegate_->OnPreStackWalk();

  profile_builder->OnSampleCompleted(
      WalkStack(module_cache_, &thread_context, stack_top, unwinders_),
      timestamp);
}

// static
std::vector<Frame> StackSamplerImpl::WalkStackForTesting(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    const base::circular_deque<std::unique_ptr<Unwinder>>& unwinders) {
  return WalkStack(module_cache, thread_context, stack_top, unwinders);
}

// static
std::vector<Frame> StackSamplerImpl::WalkStack(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    const base::circular_deque<std::unique_ptr<Unwinder>>& unwinders) {
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
    // Choose an authoritative unwinder for the current module. Use the first
    // unwinder that thinks it can unwind from the current frame.
    auto unwinder = ranges::find_if(
        unwinders, [&stack](const std::unique_ptr<Unwinder>& unwinder) {
          return unwinder->CanUnwindFrom(stack.back());
        });
    if (unwinder == unwinders.end())
      return stack;

    prior_stack_size = stack.size();
    result = unwinder->get()->TryUnwind(thread_context, stack_top, &stack);

    // The unwinder with the lowest priority should be the only one that returns
    // COMPLETED since the stack starts in native code.
    DCHECK(result != UnwindResult::COMPLETED ||
           unwinder->get() == unwinders.back().get());
  } while (result != UnwindResult::ABORTED &&
           result != UnwindResult::COMPLETED &&
           // Give up if the authoritative unwinder for the module was unable to
           // unwind.
           stack.size() > prior_stack_size);

  return stack;
}

}  // namespace base
