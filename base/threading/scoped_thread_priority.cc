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

constinit thread_local internal::ScopedBoostPriorityBase* current_boost_scope =
    nullptr;

}  // namespace

namespace internal {

ScopedBoostPriorityBase::ScopedBoostPriorityBase()
    : initial_thread_type_(current_boost_scope &&
                                   current_boost_scope->target_thread_type_
                               ? *current_boost_scope->target_thread_type_
                               : PlatformThread::GetCurrentThreadType()) {
  previous_boost_scope_ = std::exchange(current_boost_scope, this);
}

ScopedBoostPriorityBase::~ScopedBoostPriorityBase() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK_EQ(current_boost_scope, this);
  current_boost_scope = previous_boost_scope_;
}

bool ScopedBoostPriorityBase::ShouldBoostTo(
    ThreadType target_thread_type) const {
  return initial_thread_type_ < target_thread_type &&
         PlatformThread::CanChangeThreadType(initial_thread_type_,
                                             target_thread_type) &&
         PlatformThread::CanChangeThreadType(target_thread_type,
                                             initial_thread_type_);
}

bool ScopedBoostPriorityBase::CurrentThreadHasScope() {
  return current_boost_scope != nullptr;
}

}  // namespace internal

ScopedBoostPriority::ScopedBoostPriority(ThreadType target_thread_type) {
  CHECK_LT(target_thread_type, ThreadType::kRealtimeAudio);
  const bool should_boost = ShouldBoostTo(target_thread_type);
  if (should_boost) {
    target_thread_type_ = target_thread_type;
    priority_override_handle_ = internal::SetThreadTypeOverride(
        PlatformThread::CurrentHandle(), target_thread_type);
  }
}

ScopedBoostPriority::~ScopedBoostPriority() {
  if (target_thread_type_) {
    internal::RemoveThreadTypeOverride(PlatformThread::CurrentHandle(),
                                       priority_override_handle_,
                                       initial_thread_type_);
  }
}

ScopedBoostablePriority::ScopedBoostablePriority()
    : thread_handle_(PortableCurrentThreadHandle())
#if BUILDFLAG(IS_WIN)
      ,
      scoped_handle_(thread_handle_.platform_handle())
#endif
{
}

ScopedBoostablePriority::~ScopedBoostablePriority() {
  Reset();
}

bool ScopedBoostablePriority::BoostPriority(ThreadType target_thread_type) {
  CHECK_LT(target_thread_type, ThreadType::kRealtimeAudio);
  if (thread_handle_.is_null()) {
    return false;
  }
  const bool should_boost = ShouldBoostTo(target_thread_type);
  if (!should_boost) {
    return false;
  }
  if (target_thread_type_) {
    return false;
  }
  target_thread_type_ = target_thread_type;
  priority_override_handle_ =
      internal::SetThreadTypeOverride(thread_handle_, target_thread_type);
  return priority_override_handle_;
}

void ScopedBoostablePriority::Reset() {
  if (thread_handle_.is_null()) {
    return;
  }
  if (target_thread_type_) {
    internal::RemoveThreadTypeOverride(
        thread_handle_, priority_override_handle_, initial_thread_type_);
    target_thread_type_ = std::nullopt;
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

  boost_priority_.emplace(base::ThreadType::kDefault);
#endif  // BUILDFLAG(IS_WIN)
}

ScopedMayLoadLibraryAtBackgroundPriority::
    ~ScopedMayLoadLibraryAtBackgroundPriority() {
#if BUILDFLAG(IS_WIN)
  boost_priority_.reset();

  if (already_loaded_) {
    already_loaded_->store(true, std::memory_order_relaxed);
  }
#endif  // BUILDFLAG(IS_WIN)
  TRACE_EVENT_END0("base", "ScopedMayLoadLibraryAtBackgroundPriority");
}

}  // namespace internal
}  // namespace base
