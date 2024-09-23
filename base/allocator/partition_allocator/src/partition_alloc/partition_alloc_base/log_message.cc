// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_base/log_message.h"

// TODO(crbug.com/40158212): After finishing copying //base files to PA library,
// remove defined(BASE_CHECK_H_) from here.
#if defined(                                                                                 \
    BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_BASE_CHECK_H_) || \
    defined(BASE_CHECK_H_) ||                                                                \
    defined(                                                                                 \
        BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_ALLOC_CHECK_H_)
#error "log_message.h should not include check.h"
#endif

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/debug/alias.h"
#include "partition_alloc/partition_alloc_base/debug/stack_trace.h"
#include "partition_alloc/partition_alloc_base/immediate_crash.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/strings/safe_sprintf.h"
#include "partition_alloc/partition_alloc_base/strings/string_util.h"
#include "partition_alloc/partition_alloc_base/strings/stringprintf.h"

#if PA_BUILDFLAG(IS_WIN)
#include <windows.h>

#include <io.h>
#endif

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
#include <unistd.h>

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#endif

#if PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
#include "partition_alloc/partition_alloc_base/posix/safe_strerror.h"
#endif

namespace partition_alloc::internal::logging {

namespace {

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOGGING_NUM_SEVERITIES == std::size(log_severity_names),
              "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOGGING_NUM_SEVERITIES) {
    return log_severity_names[severity];
  }
  return "UNKNOWN";
}

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction g_log_message_handler = nullptr;

}  // namespace

#if PA_BUILDFLAG(DCHECK_IS_CONFIGURABLE)
// In DCHECK-enabled Chrome builds, allow the meaning of LOGGING_DCHECK to be
// determined at run-time. We default it to ERROR, to avoid it triggering
// crashes before the run-time has explicitly chosen the behaviour.
PA_COMPONENT_EXPORT(PARTITION_ALLOC_BASE)
logging::LogSeverity LOGGING_DCHECK = LOGGING_ERROR;
#endif  // PA_BUILDFLAG(DCHECK_IS_CONFIGURABLE)

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
base::strings::CStringBuilder* g_swallow_stream;

void SetLogMessageHandler(LogMessageHandlerFunction handler) {
  g_log_message_handler = handler;
}

LogMessageHandlerFunction GetLogMessageHandler() {
  return g_log_message_handler;
}

LogMessage::LogMessage(const char* file, int line, LogSeverity severity)
    : severity_(severity), file_(file), line_(line) {
  Init(file, line);
}

LogMessage::LogMessage(const char* file, int line, const char* condition)
    : severity_(LOGGING_FATAL), file_(file), line_(line) {
  Init(file, line);
  stream_ << "Check failed: " << condition << ". ";
}

LogMessage::~LogMessage() {
  stream_ << '\n';
  const char* str_newline = stream_.c_str();

  // Give any log message handler first dibs on the message.
  if (g_log_message_handler &&
      g_log_message_handler(severity_, file_, line_, message_start_,
                            str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  // Always use RawLog() if g_log_message_handler doesn't filter messages.
  RawLog(severity_, str_newline);

  // TODO(crbug.com/40213558): Enable a stack trace on a fatal on fuchsia.
#if !defined(OFFICIAL_BUILD) &&                         \
    (PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_WIN)) && \
    !defined(__UCLIBC__) && !PA_BUILDFLAG(IS_AIX)
  // TODO(crbug.com/40213558): Show a stack trace on a fatal, unless a debugger
  // is attached.
  if (severity_ == LOGGING_FATAL) {
    constexpr size_t kMaxTracesOfLoggingFatal = 32u;
    const void* traces[kMaxTracesOfLoggingFatal];
    size_t num_traces =
        base::debug::CollectStackTrace(traces, kMaxTracesOfLoggingFatal);
    base::debug::PrintStackTrace(traces, num_traces);
  }
#endif

  if (severity_ == LOGGING_FATAL) {
    PA_IMMEDIATE_CRASH();
  }
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  const char* last_slash_pos = base::strings::FindLastOf(file, "\\/");
  const char* filename = last_slash_pos ? last_slash_pos + 1 : file;

  {
    // TODO(darin): It might be nice if the columns were fixed width.
    stream_ << '[';
    // TODO(crbug.com/40158212): show process id, thread id, timestamp and so on
    // if needed.
    if (severity_ >= 0) {
      stream_ << log_severity_name(severity_);
    } else {
      stream_ << "VERBOSE" << -severity_;
    }
    stream_ << ":" << filename << "(" << line << ")] ";
  }
  message_start_ = strlen(stream_.c_str());
}

#if PA_BUILDFLAG(IS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if PA_BUILDFLAG(IS_WIN)
  return ::GetLastError();
#elif PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
  return errno;
#endif
}

void SystemErrorCodeToStream(base::strings::CStringBuilder& os,
                             SystemErrorCode error_code) {
  char buffer[256];
#if PA_BUILDFLAG(IS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             std::size(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    const char* whitespace_pos = base::strings::FindLastNotOf(msgbuf, "\n\r ");
    if (whitespace_pos) {
      size_t whitespace_index = whitespace_pos - msgbuf + 1;
      msgbuf[whitespace_index] = '\0';
    }
    base::strings::SafeSPrintf(buffer, "%s (0x%x)", msgbuf, error_code);
    os << buffer;
    return;
  }
  base::strings::SafeSPrintf(buffer,
                             "Error (0x%x) while retrieving error. (0x%x)",
                             GetLastError(), error_code);
  os << buffer;
#elif PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
  base::safe_strerror_r(error_code, buffer, sizeof(buffer));
  os << buffer << " (" << error_code << ")";
#endif  // PA_BUILDFLAG(IS_WIN)
}

#if PA_BUILDFLAG(IS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": ";
  SystemErrorCodeToStream(stream(), err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  base::debug::Alias(&last_error);
}
#elif PA_BUILDFLAG(IS_POSIX) || PA_BUILDFLAG(IS_FUCHSIA)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": ";
  SystemErrorCodeToStream(stream(), err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  base::debug::Alias(&last_error);
}
#endif  // PA_BUILDFLAG(IS_WIN)

}  // namespace partition_alloc::internal::logging
