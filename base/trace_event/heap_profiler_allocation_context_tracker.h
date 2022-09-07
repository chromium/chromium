// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_
#define BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_

#include <atomic>
#include <vector>

#include "base/base_export.h"
#include "base/trace_event/heap_profiler_allocation_context.h"

namespace base {
namespace trace_event {

// AllocationContextTracker is a thread-local object. Its main purpose is to
// keep track of context pointers for memory allocation samples. See
// |AllocationContext|.
//
// A thread-local instance of the context tracker is initialized lazily when it
// is first accessed. This might be because a context is pushed or popped, or
// because `GetContextSnapshot()` was called when an allocation occurred
class BASE_EXPORT AllocationContextTracker {
 public:
  enum class CaptureMode : int32_t {
    DISABLED,      // Don't capture anything
    NATIVE_STACK,  // Backtrace has full native backtraces from stack unwinding
  };

  // Globally sets capturing mode.
  // TODO(primiano): How to guard against *_STACK -> DISABLED -> *_STACK?
  static void SetCaptureMode(CaptureMode mode);

  // Returns global capturing mode.
  inline static CaptureMode capture_mode() {
    // A little lag after heap profiling is enabled or disabled is fine, it is
    // more important that the check is as cheap as possible when capturing is
    // not enabled, so do not issue a memory barrier in the fast path.
    if (capture_mode_.load(std::memory_order_relaxed) == CaptureMode::DISABLED)
      return CaptureMode::DISABLED;

    // In the slow path, an acquire load is required to pair with the release
    // store in |SetCaptureMode|. This is to ensure that the TLS slot for
    // the thread-local allocation context tracker has been initialized if
    // |capture_mode| returns something other than DISABLED.
    return capture_mode_.load(std::memory_order_acquire);
  }

  // Returns the thread-local instance, creating one if necessary. Returns
  // always a valid instance, unless it is called re-entrantly, in which case
  // returns nullptr in the nested calls.
  static AllocationContextTracker* GetInstanceForCurrentThread();

  // Set the thread name in the AllocationContextTracker of the current thread
  // if capture is enabled.
  static void SetCurrentThreadName(const char* name);

  AllocationContextTracker(const AllocationContextTracker&) = delete;
  AllocationContextTracker& operator=(const AllocationContextTracker&) = delete;

  // Starts and ends a new ignore scope between which the allocations are
  // ignored by the heap profiler. GetContextSnapshot() returns false when
  // allocations are ignored.
  void begin_ignore_scope() { ignore_scope_depth_++; }
  void end_ignore_scope() {
    if (ignore_scope_depth_)
      ignore_scope_depth_--;
  }

  // Pushes and pops a native stack frame onto thread local tracked stack.
  void PushNativeStackFrame(const void* pc);
  void PopNativeStackFrame(const void* pc);

  // Push and pop current task's context. A stack is used to support nested
  // tasks and the top of the stack will be used in allocation context.
  void PushCurrentTaskContext(const char* context);
  void PopCurrentTaskContext(const char* context);

  // Returns most recent task context added by ScopedTaskExecutionTracker.
  const char* TaskContext() const {
    return task_contexts_.empty() ? nullptr : task_contexts_.back();
  }

  // Fills a snapshot of the current thread-local context. Doesn't fill and
  // returns false if allocations are being ignored.
  bool GetContextSnapshot(AllocationContext* snapshot);

  ~AllocationContextTracker();

 private:
  AllocationContextTracker();

  static std::atomic<CaptureMode> capture_mode_;

  // The pseudo stack where frames are inserted PCs.
  std::vector<StackFrame> tracked_stack_;

  // The thread name is used as the first entry in the pseudo stack.
  const char* thread_name_ = nullptr;

  // Stack of tasks' contexts.
  std::vector<const char*> task_contexts_;

  uint32_t ignore_scope_depth_ = 0;
};

}  // namespace trace_event
}  // namespace base

#endif  // BASE_TRACE_EVENT_HEAP_PROFILER_ALLOCATION_CONTEXT_TRACKER_H_
