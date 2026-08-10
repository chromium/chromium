// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DICTATION_LOGGING_H_
#define CHROME_BROWSER_DICTATION_LOGGING_H_

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/raw_ref.h"
#include "base/memory/stack_allocated.h"
#include "base/timer/timer.h"

namespace content {
class BrowserContext;
}

namespace dictation {

// Buffer that collects browser log entries and flushes them periodically
// (e.g. every 500ms) to the extension via dictationPrivate.onBrowserLog.
// These logs can be viewed by navigating to
// chrome-extension://kbglekiebdohdafflpmiejhbfdmjdbbe/debug.html.
class DictationLogBuffer {
 public:
  static constexpr base::TimeDelta kFlushInterval = base::Milliseconds(500);

  explicit DictationLogBuffer(content::BrowserContext* context);
  ~DictationLogBuffer();
  DictationLogBuffer(const DictationLogBuffer&) = delete;
  DictationLogBuffer& operator=(const DictationLogBuffer&) = delete;

  void AddLog(std::string_view message);
  void Flush();

  bool empty() const { return buffer_.empty(); }

 private:
  struct LogEntry {
    double timestamp;
    std::string message;
  };

  const raw_ref<content::BrowserContext> context_;
  std::vector<LogEntry> buffer_;
  base::OneShotTimer flush_timer_;
};

// Helper stream class for VT_LOG macro to capture a single log line.
class DictationLogEntry {
  STACK_ALLOCATED();

 public:
  explicit DictationLogEntry(content::BrowserContext* context);
  ~DictationLogEntry();

  std::ostringstream& stream() { return stream_; }

 private:
  const raw_ref<content::BrowserContext> context_;
  std::ostringstream stream_;
};

}  // namespace dictation

#define VT_LOG(context) dictation::DictationLogEntry(context).stream()

#endif  // CHROME_BROWSER_DICTATION_LOGGING_H_
