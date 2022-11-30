// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/bad_message.h"

#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/trace_event/trace_event.h"
#include "content/public/browser/browser_message_filter.h"
#include "content/public/browser/render_process_host.h"

namespace bad_message {

namespace {

void LogBadMessage(BadMessageReason reason) {
  LOG(ERROR) << "Terminating renderer for bad IPC message, reason " << reason;
  base::UmaHistogramSparse("Stability.BadMessageTerminated.Chrome", reason);
}

}  // namespace

void ReceivedBadMessage(content::RenderProcessHost* host,
                        BadMessageReason reason) {
  TRACE_EVENT_INSTANT2("ipc,security", "chrome::ReceivedBadMessage",
                       TRACE_EVENT_SCOPE_THREAD, "reason", reason,
                       "render_process_host", host);
  LogBadMessage(reason);
  host->ShutdownForBadMessage(
      content::RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
}

void ReceivedBadMessage(content::BrowserMessageFilter* filter,
                        BadMessageReason reason) {
  TRACE_EVENT_INSTANT1("ipc,security", "chrome::ReceivedBadMessage",
                       TRACE_EVENT_SCOPE_THREAD, "reason", reason);
  LogBadMessage(reason);
  filter->ShutdownForBadMessage();
}

}  // namespace bad_message
