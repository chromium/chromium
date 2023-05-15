// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/thread_watcher_report_hang.h"

#include "base/debug/alias.h"
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

  // Record the time of the hang in convenient units. This can be compared to
  // times stored in places like TaskAnnotator::RunTaskImpl() and BrowserMain()
  // when analyzing hangs.
  const int64_t hang_time = base::TimeTicks::Now().since_origin().InSeconds();
  base::debug::Alias(&hang_time);

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
