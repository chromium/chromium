// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/heap_profiler_allocation_context_tracker.h"

#include <string.h>

#include <algorithm>
#include <iterator>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/cxx17_backports.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/leak_annotations.h"
#include "base/debug/stack_trace.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/platform_thread.h"
#include "base/threading/thread_local_storage.h"
#include "base/trace_event/heap_profiler_allocation_context.h"
#include "build/build_config.h"

#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE)
#include "base/trace_event/cfi_backtrace_android.h"
#endif

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include <sys/prctl.h>
#endif

namespace base {
namespace trace_event {

std::atomic<AllocationContextTracker::CaptureMode>
    AllocationContextTracker::capture_mode_{
        AllocationContextTracker::CaptureMode::DISABLED};

namespace {

const size_t kMaxStackDepth = 128u;
const size_t kMaxTaskDepth = 16u;
AllocationContextTracker* const kInitializingSentinel =
    reinterpret_cast<AllocationContextTracker*>(-1);

// This function is added to the TLS slot to clean up the instance when the
// thread exits.
void DestructAllocationContextTracker(void* alloc_ctx_tracker) {
  delete static_cast<AllocationContextTracker*>(alloc_ctx_tracker);
}

ThreadLocalStorage::Slot& AllocationContextTrackerTLS() {
  static NoDestructor<ThreadLocalStorage::Slot> tls_alloc_ctx_tracker(
      &DestructAllocationContextTracker);
  return *tls_alloc_ctx_tracker;
}

// Cannot call ThreadIdNameManager::GetName because it holds a lock and causes
// deadlock when lock is already held by ThreadIdNameManager before the current
// allocation. Gets the thread name from kernel if available or returns a string
// with id. This function intentionally leaks the allocated strings since they
// are used to tag allocations even after the thread dies.
const char* GetAndLeakThreadName() {
  char name[16];
#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
  // If the thread name is not set, try to get it from prctl. Thread name might
  // not be set in cases where the thread started before heap profiling was
  // enabled.
  int err = prctl(PR_GET_NAME, name);
  if (!err) {
    return strdup(name);
  }
#endif  // defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)

  // Use tid if we don't have a thread name.
  snprintf(name, sizeof(name), "%lu",
           static_cast<unsigned long>(PlatformThread::CurrentId()));
  return strdup(name);
}

}  // namespace

// static
AllocationContextTracker*
AllocationContextTracker::GetInstanceForCurrentThread() {
  AllocationContextTracker* tracker = static_cast<AllocationContextTracker*>(
      AllocationContextTrackerTLS().Get());
  if (tracker == kInitializingSentinel)
    return nullptr;  // Re-entrancy case.

  if (!tracker) {
    AllocationContextTrackerTLS().Set(kInitializingSentinel);
    tracker = new AllocationContextTracker();
    AllocationContextTrackerTLS().Set(tracker);
  }

  return tracker;
}

AllocationContextTracker::AllocationContextTracker() {
  tracked_stack_.reserve(kMaxStackDepth);
  task_contexts_.reserve(kMaxTaskDepth);
  task_contexts_.push_back("UntrackedTask");
}
AllocationContextTracker::~AllocationContextTracker() = default;

// static
void AllocationContextTracker::SetCurrentThreadName(const char* name) {
  if (name && capture_mode() != CaptureMode::DISABLED) {
    GetInstanceForCurrentThread()->thread_name_ = name;
  }
}

// static
void AllocationContextTracker::SetCaptureMode(CaptureMode mode) {
  // Release ordering ensures that when a thread observes |capture_mode_| to
  // be true through an acquire load, the TLS slot has been initialized.
  capture_mode_.store(mode, std::memory_order_release);
}

void AllocationContextTracker::PushNativeStackFrame(const void* pc) {
  if (tracked_stack_.size() < kMaxStackDepth)
    tracked_stack_.push_back(StackFrame::FromProgramCounter(pc));
  else
    NOTREACHED();
}

void AllocationContextTracker::PopNativeStackFrame(const void* pc) {
  if (tracked_stack_.empty())
    return;

  DCHECK_EQ(pc, tracked_stack_.back().value);
  tracked_stack_.pop_back();
}

void AllocationContextTracker::PushCurrentTaskContext(const char* context) {
  DCHECK(context);
  if (task_contexts_.size() < kMaxTaskDepth)
    task_contexts_.push_back(context);
  else
    NOTREACHED();
}

void AllocationContextTracker::PopCurrentTaskContext(const char* context) {
  // Guard for stack underflow. If tracing was started with a TRACE_EVENT in
  // scope, the context was never pushed, so it is possible that pop is called
  // on an empty stack. Note that the context always contains "UntrackedTask".
  if (task_contexts_.size() == 1)
    return;

  DCHECK_EQ(context, task_contexts_.back())
      << "Encountered an unmatched context end";
  task_contexts_.pop_back();
}

bool AllocationContextTracker::GetContextSnapshot(AllocationContext* ctx) {
  if (ignore_scope_depth_)
    return false;

  CaptureMode mode = capture_mode_.load(std::memory_order_relaxed);

  auto* backtrace = std::begin(ctx->backtrace.frames);
#if !defined(OS_NACL)
  auto* backtrace_end = std::end(ctx->backtrace.frames);
#endif

  if (!thread_name_) {
    // Ignore the string allocation made by GetAndLeakThreadName to avoid
    // reentrancy.
    ignore_scope_depth_++;
    thread_name_ = GetAndLeakThreadName();
    ANNOTATE_LEAKING_OBJECT_PTR(thread_name_);
    DCHECK(thread_name_);
    ignore_scope_depth_--;
  }

  // Add the thread name as the first entry in pseudo stack.
  if (thread_name_) {
    *backtrace++ = StackFrame::FromThreadName(thread_name_);
  }

  switch (mode) {
    case CaptureMode::DISABLED:
      {
        break;
      }
    case CaptureMode::NATIVE_STACK:
      {
// Backtrace contract requires us to return bottom frames, i.e.
// from main() and up. Stack unwinding produces top frames, i.e.
// from this point and up until main(). We intentionally request
// kMaxFrameCount + 1 frames, so that we know if there are more frames
// than our backtrace capacity.
#if !defined(OS_NACL)  // We don't build base/debug/stack_trace.cc for NaCl.
#if defined(OS_ANDROID) && BUILDFLAG(CAN_UNWIND_WITH_CFI_TABLE)
        const void* frames[Backtrace::kMaxFrameCount + 1];
        static_assert(base::size(frames) >= Backtrace::kMaxFrameCount,
                      "not requesting enough frames to fill Backtrace");
        size_t frame_count =
            CFIBacktraceAndroid::GetInitializedInstance()->Unwind(
                frames, base::size(frames));
#elif BUILDFLAG(CAN_UNWIND_WITH_FRAME_POINTERS)
        const void* frames[Backtrace::kMaxFrameCount + 1];
        static_assert(base::size(frames) >= Backtrace::kMaxFrameCount,
                      "not requesting enough frames to fill Backtrace");
        size_t frame_count = debug::TraceStackFramePointers(
            frames, base::size(frames),
            1 /* exclude this function from the trace */);
#else
        // Fall-back to capturing the stack with base::debug::StackTrace,
        // which is likely slower, but more reliable.
        base::debug::StackTrace stack_trace(Backtrace::kMaxFrameCount + 1);
        size_t frame_count = 0u;
        const void* const* frames = stack_trace.Addresses(&frame_count);
#endif

        // If there are too many frames, keep the ones furthest from main().
        size_t backtrace_capacity = backtrace_end - backtrace;
        int32_t starting_frame_index = frame_count;
        if (frame_count > backtrace_capacity) {
          starting_frame_index = backtrace_capacity - 1;
          *backtrace++ = StackFrame::FromProgramCounter(nullptr);
        }
        for (int32_t i = starting_frame_index - 1; i >= 0; --i) {
          const void* frame = frames[i];
          *backtrace++ = StackFrame::FromProgramCounter(frame);
        }
#endif  // !defined(OS_NACL)
        break;
      }
  }

  ctx->backtrace.frame_count = backtrace - std::begin(ctx->backtrace.frames);

  ctx->type_name = TaskContext();

  return true;
}

}  // namespace trace_event
}  // namespace base
