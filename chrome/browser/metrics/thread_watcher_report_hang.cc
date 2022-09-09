// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher_report_hang.h"

#include "base/debug/activity_tracker.h"
#include "base/debug/debugger.h"
#include "base/debug/dump_without_crashing.h"
#include "base/time/time.h"
#include "build/build_config.h"

namespace metrics {

// The following are unique function names for forcing the crash when a thread
// is unresponsive. This makes it possible to tell from the callstack alone what
// thread was unresponsive. Inhibiting tail calls to this function ensures that
// the caller will appear on the call stack.
NOINLINE NOT_TAIL_CALLED void ReportThreadHang() {
  [[maybe_unused]] volatile const char* inhibit_comdat = __func__;

  // The first 8 characters of sha1 of "ReportThreadHang".
  // echo -n "ReportThreadHang" | sha1sum
  static constexpr uint32_t kActivityTrackerId = 0xceec103d;

  base::debug::ScopedActivity scoped_activity(0, kActivityTrackerId, 0);
  auto& user_data = scoped_activity.user_data();
  const base::TimeTicks now = base::TimeTicks::Now();
  user_data.SetUint("timestamp_us", now.since_origin().InMicroseconds());

#if defined(NDEBUG)
  base::debug::DumpWithoutCrashing();
#else
  base::debug::BreakDebugger();
#endif
}

#if !BUILDFLAG(IS_ANDROID)

NOINLINE void ShutdownHang() {
  ReportThreadHang();
  [[maybe_unused]] volatile int inhibit_comdat = __LINE__;
}

#endif  // !BUILDFLAG(IS_ANDROID)

}  // namespace metrics
