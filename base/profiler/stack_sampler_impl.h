// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_STACK_SAMPLER_IMPL_H_
#define BASE_PROFILER_STACK_SAMPLER_IMPL_H_

#include <memory>

#include "base/base_export.h"
#include "base/callback.h"
#include "base/containers/circular_deque.h"
#include "base/profiler/frame.h"
#include "base/profiler/register_context.h"
#include "base/profiler/stack_copier.h"
#include "base/profiler/stack_sampler.h"

namespace base {

class Unwinder;

// Cross-platform stack sampler implementation. Delegates to StackCopier for the
// platform-specific stack copying implementation.
class BASE_EXPORT StackSamplerImpl : public StackSampler {
 public:
  StackSamplerImpl(std::unique_ptr<StackCopier> stack_copier,
                   UnwindersFactory core_unwinders_factory,
                   ModuleCache* module_cache,
                   RepeatingClosure record_sample_callback = RepeatingClosure(),
                   StackSamplerTestDelegate* test_delegate = nullptr);
  ~StackSamplerImpl() override;

  StackSamplerImpl(const StackSamplerImpl&) = delete;
  StackSamplerImpl& operator=(const StackSamplerImpl&) = delete;

  // StackSampler:
  void Initialize() override;
  void AddAuxUnwinder(std::unique_ptr<Unwinder> unwinder) override;
  void RecordStackFrames(StackBuffer* stack_buffer,
                         ProfileBuilder* profile_builder) override;

  // Exposes the internal function for unit testing.
  static std::vector<Frame> WalkStackForTesting(
      ModuleCache* module_cache,
      RegisterContext* thread_context,
      uintptr_t stack_top,
      const base::circular_deque<std::unique_ptr<Unwinder>>& unwinders);

 private:
  static std::vector<Frame> WalkStack(
      ModuleCache* module_cache,
      RegisterContext* thread_context,
      uintptr_t stack_top,
      const base::circular_deque<std::unique_ptr<Unwinder>>& unwinders);

  const std::unique_ptr<StackCopier> stack_copier_;
  UnwindersFactory unwinders_factory_;

  // Unwinders are stored in decreasing priority order.
  base::circular_deque<std::unique_ptr<Unwinder>> unwinders_;

  ModuleCache* const module_cache_;
  const RepeatingClosure record_sample_callback_;
  StackSamplerTestDelegate* const test_delegate_;

  // True if ownership of the object has been passed to the profiling thread and
  // initialization has occurred there. If that's the case then any further aux
  // unwinder that's provided needs to be set up within AddAuxUnwinder().
  bool was_initialized_ = false;
};

}  // namespace base

#endif  // BASE_PROFILER_STACK_SAMPLER_IMPL_H_
