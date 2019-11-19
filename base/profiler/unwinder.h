// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_UNWINDER_H_
#define BASE_PROFILER_UNWINDER_H_

#include <vector>

#include "base/macros.h"
#include "base/profiler/frame.h"
#include "base/profiler/register_context.h"
#include "base/sampling_heap_profiler/module_cache.h"

namespace base {

// The result of attempting to unwind stack frames.
enum class UnwindResult {
  // The end of the stack was reached successfully.
  COMPLETED,

  // The walk reached a frame that it doesn't know how to unwind, but might be
  // unwindable by the other native/aux unwinder.
  UNRECOGNIZED_FRAME,

  // The walk was aborted and is not resumable.
  ABORTED,
};

// Unwinder provides an interface for stack frame unwinder implementations for
// use with the StackSamplingProfiler. The profiler is expected to call
// CanUnwind() to determine if the Unwinder thinks it can unwind from the frame
// represented by the context values, then TryUnwind() to attempt the
// unwind. Note that the stack samples for multiple collection scenarios are
// interleaved on a single Unwinder instance.
class Unwinder {
 public:
  virtual ~Unwinder() = default;

  // Invoked to allow the unwinder to add any non-native modules it recognizes
  // to the ModuleCache.
  virtual void AddNonNativeModules(ModuleCache* module_cache) {}

  // Returns true if the unwinder recognizes the code referenced by
  // |current_frame| as code from which it should be able to unwind. When
  // multiple unwinders are in use, each should return true for a disjoint set
  // of frames. Note that if the unwinder returns true it may still legitmately
  // fail to unwind; e.g. in the case of a native unwind for a function that
  // doesn't have unwind information.
  virtual bool CanUnwindFrom(const Frame* current_frame) const = 0;

  // Attempts to unwind the frame represented by the context values.
  // Walks the native frames on the stack pointed to by the stack pointer in
  // |thread_context|, appending the frames to |stack|. When invoked
  // stack->back() contains the frame corresponding to the state in
  // |thread_context|.
  virtual UnwindResult TryUnwind(RegisterContext* thread_context,
                                 uintptr_t stack_top,
                                 ModuleCache* module_cache,
                                 std::vector<Frame>* stack) const = 0;

  Unwinder(const Unwinder&) = delete;
  Unwinder& operator=(const Unwinder&) = delete;

 protected:
  Unwinder() = default;
};

}  // namespace base

#endif  // BASE_PROFILER_UNWINDER_H_
