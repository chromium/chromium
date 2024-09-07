// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/profiler/stack_sampler.h"

#include <iterator>
#include <utility>

#include "base/check.h"
#include "base/compiler_specific.h"
#include "base/memory/ptr_util.h"
#include "base/memory/stack_allocated.h"
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
#include "base/task/thread_pool.h"

// IMPORTANT NOTE: Some functions within this implementation are invoked while
// the target thread is suspended so it must not do any allocation from the
// heap, including indirectly via use of DCHECK/CHECK or other logging
// statements. Otherwise this code can deadlock on heap locks acquired by the
// target thread before it was suspended. These functions are commented with "NO
// HEAP ALLOCATIONS".

namespace base {

namespace {

using CallbackRunner = base::RefCountedData<ScopedClosureRunner>;

Unwinder* GetUnwinder(const UnwinderCapture& state) {
  return std::get<0>(state);
}

UnwinderStateCapture* GetStateCapture(const UnwinderCapture& state) {
  return std::get<1>(state).get();
}

// Notifies the unwinders about the stack capture, and records metadata, while
// the thread is suspended.
class StackCopierDelegate : public StackCopier::Delegate {
  STACK_ALLOCATED();

 public:
  StackCopierDelegate(const std::vector<UnwinderCapture>* unwinders,
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
    for (const auto& unwinder : *unwinders_) {
      GetUnwinder(unwinder)->OnStackCapture(GetStateCapture(unwinder));
    }

    profile_builder_->RecordMetadata(*metadata_provider_);
  }

 private:
  const std::vector<UnwinderCapture>* unwinders_;

  ProfileBuilder* const profile_builder_;
  const MetadataRecorder::MetadataProvider* const metadata_provider_;
};

}  // namespace

StackSampler::~StackSampler() = default;

std::unique_ptr<StackBuffer> StackSampler::CreateStackBuffer() {
  size_t size = GetStackBufferSize();
  if (size == 0) {
    return nullptr;
  }
  return std::make_unique<StackBuffer>(size);
}

void StackSampler::Initialize() {
  was_initialized_ = true;
  unwind_data_->Initialize(std::move(unwinders_factory_).Run());
  thread_pool_runner_ = base::ThreadPool::CreateSequencedTaskRunner({});

  // The thread pool might not start right away (or it may never start), so we
  // schedule a job and wait for it to become running before we schedule other
  // work.
  thread_pool_runner_->PostTaskAndReply(
      FROM_HERE, base::DoNothing(),
      base::BindOnce(&StackSampler::ThreadPoolRunning,
                     weak_ptr_factory_.GetWeakPtr()));
}

void StackSampler::ThreadPoolRunning() {
  thread_pool_ready_ = true;
  unwind_data_->OnThreadPoolRunning();
}

void StackSampler::Stop(OnceClosure done_callback) {
  if (thread_pool_ready_) {
    // Post a task to the sequenced task runner to ensure we've completed any
    // remaining work. We need to ensure we use a CallbackRunner here
    // because we want to ensure `done_callback` is called even if
    // PostTaskAndReply returns false.
    auto callback_runner = base::MakeRefCounted<CallbackRunner>(
        ScopedClosureRunner(std::move(done_callback)));
    bool res = thread_pool_runner_->PostTaskAndReply(
        FROM_HERE, base::DoNothing(),
        base::BindOnce([](scoped_refptr<CallbackRunner> runner) {},
                       callback_runner));
    if (!res) {
      callback_runner->data.RunAndReset();
    }

  } else {
    std::move(done_callback).Run();
  }
}

void StackSampler::AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) {
  if (thread_pool_ready_) {
    // If we have initialized a thread pool, then we need the Initialize to
    // be called on the thread pool since it will manipulate the ModuleCache,
    // but AddAuxUnwinder needs to happen on the SamplingThread.
    thread_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](StackUnwindData* unwind_data,
               std::unique_ptr<Unwinder> unwinder) {
              unwinder->Initialize(unwind_data->module_cache());
              return unwinder;
            },
            base::Unretained(unwind_data_.get()), std::move(unwinder)),
        base::BindOnce(&StackSampler::AddAuxUnwinderWithoutInit,
                       weak_ptr_factory_.GetWeakPtr()));
  } else {
    // Initialize() invokes Initialize() on the unwinders that are present
    // at the time. If it hasn't occurred yet, we allow it to add the initial
    // modules, otherwise we do it here.
    if (was_initialized_) {
      unwinder->Initialize(unwind_data_->module_cache());
    }
    unwind_data_->AddAuxUnwinder(std::move(unwinder));
  }
}

void StackSampler::AddAuxUnwinderWithoutInit(
    std::unique_ptr<Unwinder> unwinder) {
  unwind_data_->AddAuxUnwinder(std::move(unwinder));
}

void StackSampler::RecordStackFrames(StackBuffer* stack_buffer,
                                     PlatformThreadId thread_id,
                                     base::OnceClosure done_callback) {
  DCHECK(stack_buffer);

  if (record_sample_callback_) {
    record_sample_callback_.Run();
  }

  RegisterContext thread_context;
  uintptr_t stack_top;
  TimeTicks timestamp;

  std::vector<UnwinderCapture> unwinders = unwind_data_->GetUnwinderSnapshot();
  ProfileBuilder* profile_builder = unwind_data_->profile_builder();

  bool copy_stack_succeeded;
  {
    // Make this scope as small as possible because |metadata_provider| is
    // holding a lock.
    MetadataRecorder::MetadataProvider metadata_provider(
        GetSampleMetadataRecorder(), thread_id);
    StackCopierDelegate delegate(&unwinders, profile_builder,
                                 &metadata_provider);
    copy_stack_succeeded = stack_copier_->CopyStack(
        stack_buffer, &stack_top, &timestamp, &thread_context, &delegate);
  }
  if (!copy_stack_succeeded) {
    profile_builder->OnSampleCompleted(
        {}, timestamp.is_null() ? TimeTicks::Now() : timestamp);
    std::move(done_callback).Run();
    return;
  }

  for (const auto& unwinder : unwinders) {
    GetUnwinder(unwinder)->UpdateModules(GetStateCapture(unwinder));
  }

  if (test_delegate_) {
    test_delegate_->OnPreStackWalk();
  }

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

  if (thread_pool_ready_) {
    // Since `stack_buffer` needs to be the maximum stack size and be
    // preallocated it tends to be much larger than the actual stack size. So we
    // copy the stack here that is a smaller size before passing it over to the
    // worker. To allocate a `StackBuffer` for every sample not be good.
    std::unique_ptr<StackBuffer> cloned_stack =
        stack_copier_->CloneStack(*stack_buffer, &stack_top, &thread_context);
    thread_pool_runner_->PostTaskAndReplyWithResult(
        FROM_HERE,
        base::BindOnce(
            [](StackUnwindData* unwind_data,
               std::vector<UnwinderCapture> unwinders,
               RegisterContext thread_context,
               std::unique_ptr<StackBuffer> stack, uintptr_t stack_top) {
              return WalkStack(unwind_data->module_cache(), &thread_context,
                               stack_top, std::move(unwinders));
            },
            base::Unretained(unwind_data_.get()), std::move(unwinders),
            OwnedRef(thread_context), std::move(cloned_stack), stack_top),
        base::BindOnce(&StackSampler::UnwindComplete,
                       weak_ptr_factory_.GetWeakPtr(), timestamp,
                       std::move(done_callback)));
  } else {
    auto frames = WalkStack(unwind_data_->module_cache(), &thread_context,
                            stack_top, std::move(unwinders));
    UnwindComplete(timestamp, std::move(done_callback), std::move(frames));
  }
}

void StackSampler::UnwindComplete(TimeTicks timestamp,
                                  OnceClosure done_callback,
                                  std::vector<Frame> frames) {
  unwind_data_->profile_builder()->OnSampleCompleted(std::move(frames),
                                                     timestamp);
  std::move(done_callback).Run();
}

StackUnwindData* StackSampler::GetStackUnwindData() {
  return unwind_data_.get();
}

// static
std::vector<Frame> StackSampler::WalkStackForTesting(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<UnwinderCapture> unwinders) {
  return WalkStack(module_cache, thread_context, stack_top,
                   std::move(unwinders));
}

// static
std::unique_ptr<StackSampler> StackSampler::CreateForTesting(
    std::unique_ptr<StackCopier> stack_copier,
    std::unique_ptr<StackUnwindData> stack_unwind_data,
    UnwindersFactory core_unwinders_factory,
    RepeatingClosure record_sample_callback,
    StackSamplerTestDelegate* test_delegate) {
  return base::WrapUnique(
      new StackSampler(std::move(stack_copier), std::move(stack_unwind_data),
                       std::move(core_unwinders_factory),
                       record_sample_callback, test_delegate));
}

StackSampler::StackSampler(std::unique_ptr<StackCopier> stack_copier,
                           std::unique_ptr<StackUnwindData> stack_unwind_data,
                           UnwindersFactory core_unwinders_factory,
                           RepeatingClosure record_sample_callback,
                           StackSamplerTestDelegate* test_delegate)
    : stack_copier_(std::move(stack_copier)),
      unwinders_factory_(std::move(core_unwinders_factory)),
      record_sample_callback_(std::move(record_sample_callback)),
      test_delegate_(test_delegate),
      unwind_data_(std::move(stack_unwind_data)) {
  CHECK(unwinders_factory_);
}

// static
std::vector<Frame> StackSampler::WalkStack(
    ModuleCache* module_cache,
    RegisterContext* thread_context,
    uintptr_t stack_top,
    std::vector<UnwinderCapture> unwinders) {
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
    auto unwinder =
        ranges::find_if(unwinders, [&stack](const UnwinderCapture& unwinder) {
          return GetUnwinder(unwinder)->CanUnwindFrom(stack.back());
        });
    if (unwinder == unwinders.end()) {
      return stack;
    }

    prior_stack_size = stack.size();
    result = GetUnwinder(*unwinder)->TryUnwind(
        GetStateCapture(*unwinder), thread_context, stack_top, &stack);

    // The unwinder with the lowest priority should be the only one that returns
    // COMPLETED since the stack starts in native code.
    DCHECK(result != UnwindResult::kCompleted || *unwinder == unwinders.back());
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
