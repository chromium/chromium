// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/scoped_thread_priority.h"

#include "base/location.h"
#include "base/threading/platform_thread.h"
#include "base/trace_event/base_tracing.h"
#include "build/build_config.h"

namespace base {
namespace internal {

ScopedMayLoadLibraryAtBackgroundPriority::
    ScopedMayLoadLibraryAtBackgroundPriority(const Location& from_here,
                                             std::atomic_bool* already_loaded)
#if defined(OS_WIN)
    : already_loaded_(already_loaded)
#endif  // OS_WIN
{
  TRACE_EVENT_BEGIN(
      "base", "ScopedMayLoadLibraryAtBackgroundPriority",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_source_location_iid(
            base::trace_event::InternedSourceLocation::Get(
                &ctx, base::trace_event::TraceSourceLocation(from_here)));
      });

#if defined(OS_WIN)
  if (already_loaded_ && already_loaded_->load(std::memory_order_relaxed))
    return;

  const base::ThreadPriority priority =
      PlatformThread::GetCurrentThreadPriority();
  if (priority == base::ThreadPriority::BACKGROUND) {
    original_thread_priority_ = priority;
    PlatformThread::SetCurrentThreadPriority(base::ThreadPriority::NORMAL);

    TRACE_EVENT_BEGIN0(
        "base",
        "ScopedMayLoadLibraryAtBackgroundPriority : Priority Increased");
  }
#endif  // OS_WIN
}

ScopedMayLoadLibraryAtBackgroundPriority::
    ~ScopedMayLoadLibraryAtBackgroundPriority() {
  // Trace events must be closed in reverse order of opening so that they nest
  // correctly.
#if defined(OS_WIN)
  if (original_thread_priority_) {
    TRACE_EVENT_END0(
        "base",
        "ScopedMayLoadLibraryAtBackgroundPriority : Priority Increased");
    PlatformThread::SetCurrentThreadPriority(original_thread_priority_.value());
  }

  if (already_loaded_)
    already_loaded_->store(true, std::memory_order_relaxed);
#endif  // OS_WIN
  TRACE_EVENT_END0("base", "ScopedMayLoadLibraryAtBackgroundPriority");
}

}  // namespace internal
}  // namespace base
