// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LOGGING_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LOGGING_H_

#include "base/logging.h"

// Contract:
// - Thread safety: safe to call from any thread. NM_LOG expands to a plain
//   LOG()/VLOG() stream, and base/logging.cc's LogMessage::Flush() already
//   synchronizes writes to the shared log file across threads (a lock on
//   POSIX, atomic append on Windows, see base/logging.cc's GetLoggingLock()
//   and InitializeLogFileHandle()), so NM_LOG adds no state of its own and
//   is exactly as safe as an ordinary LOG() call.
// - Severity: WARNING and ERROR only. NM_LOG(FATAL) compiles, and
//   LOG_IS_ON(FATAL) is always true, so it would terminate the process on
//   every hit regardless of --log-level or --vmodule. Use CHECK() when that
//   is what you want.
// - Ownership and lifetime: not applicable. NM_LOG is a macro, not an
//   object.
// - Scope: local to chrome/browser/extensions/api/messaging. This is not a
//   base facility and is not meant to be used outside this directory.
// - Effect is per call site: only a line written as NM_LOG(...) gets this
//   behavior. It has no effect on any LOG() call that was not converted.
//
// Logs at `severity` (WARNING, ERROR) during standard logging, OR when
// verbosity is enabled for native messaging modules via --vmodule
// (for example: --log-level=3 --vmodule=*/extensions/api/messaging/*=1 to
// isolate native messaging logs while silencing the rest of the browser).
#define NM_LOG(severity) \
  LAZY_STREAM(LOG_STREAM(severity), LOG_IS_ON(severity) || VLOG_IS_ON(1))

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_NATIVE_MESSAGING_LOGGING_H_
