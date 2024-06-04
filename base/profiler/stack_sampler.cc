// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/raw_ptr.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/profiler/metadata_recorder.h"
#include "base/profiler/profile_builder.h"
#include "base/profiler/sample_metadata.h"
#include "base/profiler/stack_buffer.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/suspendable_thread_delegate.h"
#include "base/profiler/unwinder.h"
#include "base/ranges/algorithm.h"

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
  raw_ptr<const base::circular_deque<std::unique_ptr<Unwinder>>> unwinders_;
  const raw_ptr<ProfileBuilder> profile_builder_;
  const raw_ptr<const MetadataRecorder::MetadataProvider> metadata_provider_;
};

}  // namespace

StackSampler::~StackSampler() = default;

std::unique_ptr<StackBuffer> StackSampler::CreateStackBuffer() {
  size_t size = GetStackBufferSize();
  if (size == 0)
    return nullptr;
  return std::make_unique<StackBuffer>(size);
}

void StackSampler::Initialize() {
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

void StackSampler::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  // Initialize() invokes Initialize() on the unwinders that are present
  // at the time. If it hasn't occurred yet, we allow it to add the initial
  // modules, otherwise we do it here.
  if (was_initialized_)
    unwinder->Initialize(module_cache_);
  unwinders_.push_front(std::move(unwinder));
}

void StackSampler::RecordStackFrames(StackBuffer* stack_buffer,
                                     ProfileBuilder* profile_builder,
                                     PlatformThreadId thread_id) {
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
        GetSampleMetadataRecorder(), thread_id);
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

#if BUILDFLAG(IS_CHROMEOS)
  ptrdiff_t stack_size = reinterpret_cast<uint8_t*>(stack_top) -
                         reinterpret_cast<uint8_t*>(stack_buffer->buffer());
  constexpr int kBytesPerKilobyte = 1024;

  if ((++stack_size_histogram_sampling_counter_ %
       kUMAHistogramDownsampleAmount) == 0) {
    // Record the size of the stack to tune kLargeStackSize.
    // UmaHistogramMemoryKB has a min of 1000, which isn't useful for our
    // purposes, so call UmaHistogramCustomCounts directly.
    // Min is 4KB, since that's the normal pagesize and setting kLargeStackSize
    // smaller than that would be pointless. Max is 8MB since that's the
    // current ChromeOS stack size; we shouldn't be able to get a number
    // larger than that.
    UmaHistogramCustomCounts(
        "Memory.StackSamplingProfiler.StackSampleSize2",
        saturated_cast<int>(stack_size / kBytesPerKilobyte), 4, 8 * 1024, 50);
  }

  // We expect to very rarely see stacks larger than kLargeStackSize. If we see
  // a stack larger than kLargeStackSize, we tell the kernel to discard the
  // contents of the buffer (using madvise(MADV_DONTNEED)) after the first
  // kLargeStackSize bytes to avoid permanently allocating memory that we won't
  // use again. We don't want kLargeStackSize to be too small, however; for if
  // we are constantly calling madvise(MADV_DONTNEED) and then writing to the
  // same parts of the buffer, we're not saving memory and we'll cause extra
  // page faults.
  constexpr ptrdiff_t kLargeStackSize = 32 * kBytesPerKilobyte;
  if (stack_size > kLargeStackSize) {
    stack_buffer->MarkUpperBufferContentsAsUnneeded(kLargeStackSize);
  }
#endif  // #if BUILDFLAG(IS_CHROMEOS)
}

// static
std::vector<Frame> StackSampler::WalkStackForTesting(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    const base::circular_deque<std::unique_ptr<Unwinder>>& unwinders) {
  return WalkStack(module_cache, thread_context, stack_top, unwinders);
}

// static
std::unique_ptr<StackSampler> StackSampler::CreateForTesting(
    std::unique_ptr<StackCopier> stack_copier,
    UnwindersFactory core_unwinders_factory,
    ModuleCache* module_cache,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  return base::WrapUnique(new StackSampler(
      std::move(stack_copier), std::move(core_unwinders_factory), module_cache,
      record_sample_callback, test_delegate));
}

StackSampler::StackSampler(std::unique_ptr<StackCopier> stack_copier,
                           UnwindersFactory core_unwinders_factory,
                           ModuleCache* module_cache,
                           RepeatingClosure record_sample_callback,
                           StackSamplerTestDelegate* test_delegate)
    : stack_copier_(std::move(stack_copier)),
      unwinders_factory_(std::move(core_unwinders_factory)),
      module_cache_(module_cache),
      record_sample_callback_(std::move(record_sample_callback)),
      test_delegate_(test_delegate) {
  CHECK(unwinders_factory_);
}

// static
std::vector<Frame> StackSampler::WalkStack(
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
    DCHECK(result != UnwindResult::kCompleted ||
           unwinder->get() == unwinders.back().get());
  } while (result != UnwindResult::kAborted &&
           result != UnwindResult::kCompleted &&
           // Give up if the authoritative unwinder for the module was unable to
           // unwind.
           stack.size() > prior_stack_size);

  return stack;
}

StackSamplerTestDelegate::~StackSamplerTestDelegate() = default;

StackSamplerTestDelegate::StackSamplerTestDelegate() = default;

}  // namespace base
