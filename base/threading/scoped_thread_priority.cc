// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/location.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {

ScopedBoostPriority::ScopedBoostPriority(ThreadType target_thread_type) {
  DCHECK_LT(target_thread_type, ThreadType::kRealtimeAudio);
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
  if (original_thread_type_.has_value())
    PlatformThread::SetCurrentThreadType(original_thread_type_.value());
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
  if (already_loaded_ && already_loaded_->load(std::memory_order_relaxed))
    return;

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

  if (already_loaded_)
    already_loaded_->store(true, std::memory_order_relaxed);
#endif  // BUILDFLAG(IS_WIN)
  TRACE_EVENT_END0("base", "ScopedMayLoadLibraryAtBackgroundPriority");
}

}  // namespace internal
}  // namespace base
