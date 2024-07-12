// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLER_H_
#define BASE_PROFILER_STACK_SAMPLER_H_

#include <memory>
#include <tuple>
#include <vector>

#include "base/base_export.h"
#include "base/containers/circular_deque.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/profiler/frame.h"
#include "base/profiler/register_context.h"
#include "base/profiler/sampling_profiler_thread_token.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/stack_sampler.h"
#include "base/threading/platform_thread.h"
#include "build/build_config.h"

namespace base {

class ModuleCache;
class ProfileBuilder;
class StackBuffer;
class StackSamplerTestDelegate;
class Unwinder;
class UnwinderStateCapture;

using UnwinderCapture =
    std::tuple<raw_ptr<Unwinder>, std::unique_ptr<UnwinderStateCapture>>;

// StackSampler is an implementation detail of StackSamplingProfiler. It
// abstracts the native implementation required to record a set of stack frames
// for a given thread. It delegates to StackCopier for the
// platform-specific stack copying implementation.
class BASE_EXPORT StackSampler {
 public:
  // Factory for generating a set of Unwinders for use by the profiler.
  using UnwindersFactory =
      OnceCallback<std::vector<std::unique_ptr<Unwinder>>()>;

  // Creates a stack sampler that records samples for thread with
  // |thread_token|. Unwinders in |unwinders| must be stored in increasing
  // priority to guide unwind attempts. Only the unwinder with the lowest
  // priority is allowed to return with UnwindResult::kCompleted. Returns null
  // if this platform does not support stack sampling.
  static std::unique_ptr<StackSampler> Create(
      SamplingProfilerThreadToken thread_token,
      ModuleCache* module_cache,
      UnwindersFactory core_unwinders_factory,
      RepeatingClosure record_sample_callback,
      StackSamplerTestDelegate* test_delegate);

  ~StackSampler();

  StackSampler(const StackSampler&) = delete;
  StackSampler& operator=(const StackSampler&) = delete;

  // Gets the required size of the stack buffer.
  static size_t GetStackBufferSize();

  // Creates an instance of the a stack buffer that can be used for calls to
  // any StackSampler object.
  static std::unique_ptr<StackBuffer> CreateStackBuffer();

  // The following functions are all called on the SamplingThread (not the
  // thread being sampled).

  // Performs post-construction initialization on the SamplingThread.
  void Initialize();

  // Adds an auxiliary unwinder to handle additional, non-native-code unwind
  // scenarios. Unwinders must be inserted in increasing priority, following
  // |unwinders| provided in Create(), to guide unwind attempts.
  void AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder);

  // Records a set of frames and returns them.
  void RecordStackFrames(StackBuffer* stack_buffer,
                         ProfileBuilder* profile_builder,
                         PlatformThreadId thread_id);

  // Exposes the internal function for unit testing.
  static std::vector<Frame> WalkStackForTesting(
      ModuleCache* module_cache,
      RegisterContext* thread_context,
      uintptr_t stack_top,
      const std::vector<UnwinderCapture>& unwinders);

  // Create a StackSampler, overriding the platform-specific components.
  static std::unique_ptr<StackSampler> CreateForTesting(
      std::unique_ptr<StackCopier> stack_copier,
      UnwindersFactory core_unwinders_factory,
      ModuleCache* module_cache,
      RepeatingClosure record_sample_callback = RepeatingClosure(),
      StackSamplerTestDelegate* test_delegate = nullptr);

#if BUILDFLAG(IS_CHROMEOS)
  // How often to record the "Memory.StackSamplingProfiler.StackSampleSize2" UMA
  // histogram. Specifically, only 1 in kUMAHistogramDownsampleAmount calls to
  // RecordStackFrames will add a sample to the histogram. RecordStackFrames is
  // called many times a second. We don't need multiple samples per second to
  // get a good understanding of average stack sizes, and it's a lot of data to
  // record. kUMAHistogramDownsampleAmount should give us about 1 sample per 10
  // seconds per process, which is plenty. 199 is prime which should avoid any
  // aliasing issues (e.g. if stacks are larger on second boundaries or some
  // such weirdness).
  static constexpr uint32_t kUMAHistogramDownsampleAmount = 199;
#endif

 private:
  StackSampler(std::unique_ptr<StackCopier> stack_copier,
               UnwindersFactory core_unwinders_factory,
               ModuleCache* module_cache,
               RepeatingClosure record_sample_callback,
               StackSamplerTestDelegate* test_delegate);

  static std::vector<Frame> WalkStack(
      ModuleCache* module_cache,
      RegisterContext* thread_context,
      uintptr_t stack_top,
      const std::vector<UnwinderCapture>& unwinders);

  const std::unique_ptr<StackCopier> stack_copier_;
  UnwindersFactory unwinders_factory_;

  // Unwinders are stored in decreasing priority order.
  base::circular_deque<std::unique_ptr<Unwinder>> unwinders_;

  const raw_ptr<ModuleCache> module_cache_;
  const RepeatingClosure record_sample_callback_;
  const raw_ptr<StackSamplerTestDelegate> test_delegate_;

#if BUILDFLAG(IS_CHROMEOS)
  // Counter for "Memory.StackSamplingProfiler.StackSampleSize2" UMA histogram.
  // See comments above kUMAHistogramDownsampleAmount. Unsigned so that overflow
  // isn't undefined behavior.
  uint32_t stack_size_histogram_sampling_counter_ = 0;
#endif

  // True if ownership of the object has been passed to the profiling thread and
  // initialization has occurred there. If that's the case then any further aux
  // unwinder that's provided needs to be set up within AddAuxUnwinder().
  bool was_initialized_ = false;
};

// StackSamplerTestDelegate provides seams for test code to execute during stack
// collection.
class BASE_EXPORT StackSamplerTestDelegate {
 public:
  StackSamplerTestDelegate(const StackSamplerTestDelegate&) = delete;
  StackSamplerTestDelegate& operator=(const StackSamplerTestDelegate&) = delete;

  virtual ~StackSamplerTestDelegate();

  // Called after copying the stack and resuming the target thread, but prior to
  // walking the stack. Invoked on the SamplingThread.
  virtual void OnPreStackWalk() = 0;

 protected:
  StackSamplerTestDelegate();
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLER_H_
