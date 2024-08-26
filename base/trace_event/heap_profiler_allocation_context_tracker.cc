// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/heap_profiler_allocation_context_tracker.h"

#include <ostream>

#include "base/atomicops.h"
#include "base/check_op.h"
#include "base/no_destructor.h"
#include "base/notreached.h"
#include "base/threading/thread_local_storage.h"

namespace base {
namespace trace_event {

std::atomic<AllocationContextTracker::CaptureMode>
    AllocationContextTracker::capture_mode_{
        AllocationContextTracker::CaptureMode::kDisabled};

namespace {

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
  task_contexts_.reserve(kMaxTaskDepth);
  task_contexts_.push_back("UntrackedTask");
}
AllocationContextTracker::~AllocationContextTracker() = default;

// static
void AllocationContextTracker::SetCurrentThreadName(const char* name) {
  if (name && capture_mode() != CaptureMode::kDisabled) {
    GetInstanceForCurrentThread()->thread_name_ = name;
  }
}

// static
void AllocationContextTracker::SetCaptureMode(CaptureMode mode) {
  // Release ordering ensures that when a thread observes |capture_mode_| to
  // be true through an acquire load, the TLS slot has been initialized.
  capture_mode_.store(mode, std::memory_order_release);
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

}  // namespace trace_event
}  // namespace base
