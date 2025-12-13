// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/check_op.h"
#include "base/location.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/trace_event.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

namespace base {

namespace {

PlatformThreadHandle PortableCurrentThreadHandle() {
  PlatformThreadHandle current = PlatformThread::CurrentHandle();
#if BUILDFLAG(IS_WIN)
  PlatformThreadHandle::Handle platform_handle;
  BOOL did_dup = ::DuplicateHandle(
      ::GetCurrentProcess(), current.platform_handle(), ::GetCurrentProcess(),
      &platform_handle, 0, false, DUPLICATE_SAME_ACCESS);
  if (!did_dup) {
    return PlatformThreadHandle();
  }
  return PlatformThreadHandle(platform_handle);
#else
  return current;
#endif
}

}  // namespace

ScopedBoostPriority::ScopedBoostPriority(ThreadType target_thread_type) {
  CHECK_LT(target_thread_type, ThreadType::kRealtimeAudio);
  const ThreadType original_thread_type =
      PlatformThread::GetCurrentThreadType();
  const bool should_boost = original_thread_type < target_thread_type &&
                            PlatformThread::CanChangeThreadType(
                                original_thread_type, target_thread_type) &&
                            PlatformThread::CanChangeThreadType(
                                target_thread_type, original_thread_type);
  if (should_boost) {
    original_thread_type_.emplace(original_thread_type);
    PlatformThread::SetCurrentThreadType(target_thread_type);
  }
}

ScopedBoostPriority::~ScopedBoostPriority() {
  if (original_thread_type_.has_value()) {
    PlatformThread::SetCurrentThreadType(original_thread_type_.value());
  }
}

ScopedBoostablePriority::ScopedBoostablePriority()
    : initial_thread_type_(PlatformThread::GetCurrentThreadType()),
      thread_handle_(PortableCurrentThreadHandle())
#if BUILDFLAG(IS_WIN)
      ,
      scoped_handle_(thread_handle_.platform_handle())
#endif
{
}

ScopedBoostablePriority::~ScopedBoostablePriority() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (thread_handle_.is_null()) {
    return;
  }
  if (did_override_priority_) {
    internal::RemoveThreadTypeOverride(priority_override_handle_);
  }
}

bool ScopedBoostablePriority::BoostPriority(ThreadType target_thread_type) {
  CHECK_LT(target_thread_type, ThreadType::kRealtimeAudio);
  if (thread_handle_.is_null()) {
    return false;
  }
  const bool should_boost = target_thread_type > initial_thread_type_ &&
                            PlatformThread::CanChangeThreadType(
                                initial_thread_type_, target_thread_type) &&
                            PlatformThread::CanChangeThreadType(
                                target_thread_type, initial_thread_type_);
  if (!should_boost) {
    return false;
  }
  if (did_override_priority_) {
    return false;
  }
  did_override_priority_ = true;
  priority_override_handle_ =
      internal::SetThreadTypeOverride(thread_handle_, target_thread_type);
  return priority_override_handle_;
}

TaskMonitoringScopedBoostPriority::TaskMonitoringScopedBoostPriority(
    ThreadType target_thread_type,
    RepeatingCallback<bool()> should_boost_callback)
    : target_thread_type_(target_thread_type),
      should_boost_callback_(std::move(should_boost_callback)) {
  CHECK(should_boost_callback_);
}

TaskMonitoringScopedBoostPriority::~TaskMonitoringScopedBoostPriority() {
  scoped_boost_priority_.reset();
}

void TaskMonitoringScopedBoostPriority::WillProcessTask(
    const PendingTask& pending_task,
    bool was_blocked_or_low_priority) {
  bool should_boost = should_boost_callback_.Run();
  if (scoped_boost_priority_.has_value() == should_boost) {
    return;
  }

  if (should_boost) {
    scoped_boost_priority_.emplace(target_thread_type_);
  } else {
    scoped_boost_priority_.reset();
  }
}

namespace internal {

ScopedMayLoadLibraryAtBackgroundPriority::
    ScopedMayLoadLibraryAtBackgroundPriority(const Location& from_here,
                                             std::atomic_bool* already_loaded)
#if BUILDFLAG(IS_WIN)
    : already_loaded_(already_loaded)
#endif  // BUILDFLAG(IS_WIN)
{
  TRACE_EVENT_BEGIN(
      "base", "ScopedMayLoadLibraryAtBackgroundPriority",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(&ctx, from_here));
      });

#if BUILDFLAG(IS_WIN)
  if (already_loaded_ && already_loaded_->load(std::memory_order_relaxed)) {
    return;
  }

  const base::ThreadType thread_type = PlatformThread::GetCurrentThreadType();
  if (thread_type == base::ThreadType::kBackground) {
    original_thread_type_ = thread_type;
    PlatformThread::SetCurrentThreadType(base::ThreadType::kDefault);

    TRACE_EVENT_BEGIN0(
        "base",
        "ScopedMayLoadLibraryAtBackgroundPriority : Priority Increased");
  }
#endif  // BUILDFLAG(IS_WIN)
}

ScopedMayLoadLibraryAtBackgroundPriority::
    ~ScopedMayLoadLibraryAtBackgroundPriority() {
  // Trace events must be closed in reverse order of opening so that they nest
  // correctly.
#if BUILDFLAG(IS_WIN)
  if (original_thread_type_) {
    TRACE_EVENT_END0(
        "base",
        "ScopedMayLoadLibraryAtBackgroundPriority : Priority Increased");
    PlatformThread::SetCurrentThreadType(original_thread_type_.value());
  }

  if (already_loaded_) {
    already_loaded_->store(true, std::memory_order_relaxed);
  }
#endif  // BUILDFLAG(IS_WIN)
  TRACE_EVENT_END0("base", "ScopedMayLoadLibraryAtBackgroundPriority");
}

}  // namespace internal
}  // namespace base
