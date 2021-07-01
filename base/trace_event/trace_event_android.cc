// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/trace_event_impl.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/format_macros.h"
#include "base/logging.h"
#include "base/posix/eintr_wrapper.h"
#include "base/strings/stringprintf.h"
#include "base/synchronization/waitable_event.h"
#include "base/threading/thread.h"
#include "base/trace_event/trace_event.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace trace_event {

namespace {

int g_atrace_fd = -1;
const char kATraceMarkerFile[] = "/sys/kernel/tracing/trace_marker";
const char kLegacyATraceMarkerFile[] = "/sys/kernel/debug/tracing/trace_marker";

void WriteToATrace(int fd, const char* buffer, size_t size) {
  size_t total_written = 0;
  while (total_written < size) {
    ssize_t written = HANDLE_EINTR(write(
        fd, buffer + total_written, size - total_written));
    if (written <= 0)
      break;
    total_written += written;
  }
  // Tracing might have been disabled before we were notified about it, which
  // triggers EBADF. Since enabling and disabling atrace is racy, ignore the
  // error in that case to avoid logging an error for every trace event.
  if (total_written < size && errno != EBADF) {
    PLOG(WARNING) << "Failed to write buffer '" << std::string(buffer, size)
                  << "' to trace_marker";
  }
}

void WriteEvent(char phase,
                const char* category_group,
                const char* name,
                unsigned long long id,
                const TraceArguments& args,
                unsigned int flags) {
  std::string out = StringPrintf("%c|%d|%s", phase, getpid(), name);
  if (flags & TRACE_EVENT_FLAG_HAS_ID)
    StringAppendF(&out, "-%" PRIx64, static_cast<uint64_t>(id));
  out += '|';

  const char* const* arg_names = args.names();
  for (size_t i = 0; i < args.size() && arg_names[i]; ++i) {
    if (i)
      out += ';';
    out += arg_names[i];
    out += '=';
    std::string::size_type value_start = out.length();
    args.values()[i].AppendAsJSON(args.types()[i], &out);

    // Remove the quotes which may confuse the atrace script.
    ReplaceSubstringsAfterOffset(&out, value_start, "\\\"", "'");
    ReplaceSubstringsAfterOffset(&out, value_start, "\"", "");
    // Replace chars used for separators with similar chars in the value.
    std::replace(out.begin() + value_start, out.end(), ';', ',');
    std::replace(out.begin() + value_start, out.end(), '|', '!');
  }

  out += '|';
  out += category_group;
  WriteToATrace(g_atrace_fd, out.c_str(), out.size());
}

int OpenATraceMarkerFile(int mode) {
  int fd = HANDLE_EINTR(open(kATraceMarkerFile, mode));
  if (fd == -1)
    fd = HANDLE_EINTR(open(kLegacyATraceMarkerFile, mode));
  if (fd == -1) {
    PLOG(WARNING) << "Couldn't open " << kATraceMarkerFile << " or "
                  << kLegacyATraceMarkerFile;
    return -1;
  }
  return fd;
}

}  // namespace

// These functions support Android systrace.py when 'webview' category is
// traced. With the new adb_profile_chrome, we may have two phases:
// - before WebView is ready for combined tracing, we can use adb_profile_chrome
//   to trace android categories other than 'webview' and chromium categories.
//   In this way we can avoid the conflict between StartATrace/StopATrace and
//   the intents.
// - TODO(wangxianzhu): after WebView is ready for combined tracing, remove
//   StartATrace, StopATrace and SendToATrace, and perhaps send Java traces
//   directly to atrace in trace_event_binding.cc.

void TraceLog::StartATrace(const std::string& category_filter) {
  if (g_atrace_fd != -1)
    return;

  g_atrace_fd = OpenATraceMarkerFile(O_WRONLY);
  if (g_atrace_fd == -1)
    return;
  TraceConfig trace_config(category_filter);
  trace_config.SetTraceRecordMode(RECORD_CONTINUOUSLY);
  SetEnabled(trace_config, TraceLog::RECORDING_MODE);
}

void TraceLog::StopATrace() {
  if (g_atrace_fd != -1) {
    close(g_atrace_fd);
    g_atrace_fd = -1;
  }
  SetDisabled();
}

void TraceEvent::SendToATrace() {
  if (g_atrace_fd == -1)
    return;

  const char* category_group =
      TraceLog::GetCategoryGroupName(category_group_enabled_);

  switch (phase_) {
    case TRACE_EVENT_PHASE_BEGIN:
      WriteEvent('B', category_group, name_, id_, args_, flags_);
      break;

    case TRACE_EVENT_PHASE_COMPLETE:
      WriteEvent(duration_.ToInternalValue() == -1 ? 'B' : 'E', category_group,
                 name_, id_, args_, flags_);
      break;

    case TRACE_EVENT_PHASE_END:
      // Though a single 'E' is enough, here append pid, name and
      // category_group etc. So that unpaired events can be found easily.
      WriteEvent('E', category_group, name_, id_, args_, flags_);
      break;

    case TRACE_EVENT_PHASE_INSTANT:
      // Simulate an instance event with a pair of begin/end events.
      WriteEvent('B', category_group, name_, id_, args_, flags_);
      WriteToATrace(g_atrace_fd, "E", 1);
      break;

    case TRACE_EVENT_PHASE_COUNTER:
      for (size_t i = 0; i < arg_size() && arg_name(i); ++i) {
        DCHECK(arg_type(i) == TRACE_VALUE_TYPE_INT);
        std::string out =
            base::StringPrintf("C|%d|%s-%s", getpid(), name_, arg_name(i));
        if (flags_ & TRACE_EVENT_FLAG_HAS_ID)
          StringAppendF(&out, "-%" PRIx64, static_cast<uint64_t>(id_));
        StringAppendF(&out, "|%d|%s", static_cast<int>(arg_value(i).as_int),
                      category_group);
        WriteToATrace(g_atrace_fd, out.c_str(), out.size());
      }
      break;

    default:
      // Do nothing.
      break;
  }
}

void TraceLog::AddClockSyncMetadataEvent() {
  int atrace_fd = OpenATraceMarkerFile(O_WRONLY | O_APPEND);
  if (atrace_fd == -1)
    return;

  // Android's kernel trace system has a trace_marker feature: this is a file on
  // debugfs that takes the written data and pushes it onto the trace
  // buffer. So, to establish clock sync, we write our monotonic clock into that
  // trace buffer.
  double now_in_seconds = (TRACE_TIME_TICKS_NOW() - TimeTicks()).InSecondsF();
  std::string marker = StringPrintf(
      "trace_event_clock_sync: parent_ts=%f\n", now_in_seconds);
  WriteToATrace(atrace_fd, marker.c_str(), marker.size());
  close(atrace_fd);
}

void TraceLog::SetupATraceStartupTrace(const std::string& category_filter) {
  atrace_startup_config_ = TraceConfig(category_filter);
}

absl::optional<TraceConfig> TraceLog::TakeATraceStartupConfig() {
  return std::move(atrace_startup_config_);
}

}  // namespace trace_event
}  // namespace base
