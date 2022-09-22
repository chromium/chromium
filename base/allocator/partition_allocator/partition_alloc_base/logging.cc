// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/logging.h"

// TODO(1151236): After finishing copying //base files to PA library, remove
// defined(BASE_CHECK_H_) from here.
#if defined(                                                             \
    BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_BASE_CHECK_H_) || \
    defined(BASE_CHECK_H_) ||                                            \
    defined(BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ALLOC_CHECK_H_)
#error "logging.h should not include check.h"
#endif

#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_base/debug/alias.h"
#include "base/allocator/partition_allocator/partition_alloc_base/immediate_crash.h"
#include "base/allocator/partition_allocator/partition_alloc_base/strings/stringprintf.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_WIN)

#include <io.h>
#include <windows.h>
// Windows warns on using write().  It prefers _write().
#define write(fd, buf, count) _write(fd, buf, static_cast<unsigned int>(count))
// Windows doesn't define STDERR_FILENO.  Define it here.
#define STDERR_FILENO 2

#endif

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#endif

#include <cstring>
#include <ostream>
#include <string>

#include "base/allocator/partition_allocator/partition_alloc_base/posix/eintr_wrapper.h"

#if BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
#include "base/allocator/partition_allocator/partition_alloc_base/posix/safe_strerror.h"
#endif

namespace partition_alloc::internal::logging {

namespace {

const char* const log_severity_names[] = {"INFO", "WARNING", "ERROR", "FATAL"};
static_assert(LOGGING_NUM_SEVERITIES == std::size(log_severity_names),
              "Incorrect number of log_severity_names");

const char* log_severity_name(int severity) {
  if (severity >= 0 && severity < LOGGING_NUM_SEVERITIES)
    return log_severity_names[severity];
  return "UNKNOWN";
}

int g_min_log_level = 0;

// A log message handler that gets notified of every log message we process.
LogMessageHandlerFunction g_log_message_handler = nullptr;

#if !BUILDFLAG(IS_WIN)
void WriteToStderr(const char* data, size_t length) {
  size_t bytes_written = 0;
  int rv;
  while (bytes_written < length) {
    rv = PA_HANDLE_EINTR(
        write(STDERR_FILENO, data + bytes_written, length - bytes_written));
    if (rv < 0) {
      // Give up, nothing we can do now.
      break;
    }
    bytes_written += rv;
  }
}
#else   // !BUILDFLAG(IS_WIN)
void WriteToStderr(const char* data, size_t length) {
  HANDLE handle = ::GetStdHandle(STD_ERROR_HANDLE);
  const char* ptr = data;
  const char* ptr_end = data + length;
  while (ptr < ptr_end) {
    DWORD bytes_written = 0;
    if (!::WriteFile(handle, ptr, ptr_end - ptr, &bytes_written, nullptr) ||
        bytes_written == 0) {
      // Give up, nothing we can do now.
      break;
    }
    ptr += bytes_written;
  }
}
#endif  // !BUILDFLAG(IS_WIN)

}  // namespace

#if BUILDFLAG(PA_DCHECK_IS_CONFIGURABLE)
// In DCHECK-enabled Chrome builds, allow the meaning of LOGGING_DCHECK to be
// determined at run-time. We default it to INFO, to avoid it triggering
// crashes before the run-time has explicitly chosen the behaviour.
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
logging::LogSeverity LOGGING_DCHECK = LOGGING_INFO;
#endif  // BUILDFLAG(PA_DCHECK_IS_CONFIGURABLE)

// This is never instantiated, it's just used for EAT_STREAM_PARAMETERS to have
// an object of the correct type on the LHS of the unused part of the ternary
// operator.
std::ostream* g_swallow_stream;

void SetMinLogLevel(int level) {
  g_min_log_level = std::min(LOGGING_FATAL, level);
}

int GetMinLogLevel() {
  return g_min_log_level;
}

bool ShouldCreateLogMessage(int severity) {
  if (severity < g_min_log_level)
    return false;

  // Return true here unless we know ~LogMessage won't do anything.
  return true;
}

int GetVlogVerbosity() {
  return std::max(-1, LOG_INFO - GetMinLogLevel());
}

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
  stream_ << std::endl;
  std::string str_newline(stream_.str());

  // Give any log message handler first dibs on the message.
  if (g_log_message_handler &&
      g_log_message_handler(severity_, file_, line_, message_start_,
                            str_newline)) {
    // The handler took care of it, no further processing.
    return;
  }

  // Always use RawLog() if g_log_message_handler doesn't filter messages.
  RawLog(severity_, str_newline.c_str());
}

// writes the common header info to the stream
void LogMessage::Init(const char* file, int line) {
  std::string filename(file);
  size_t last_slash_pos = filename.find_last_of("\\/");
  if (last_slash_pos != std::string::npos)
    filename.erase(0, last_slash_pos + 1);

  {
    // TODO(darin): It might be nice if the columns were fixed width.
    stream_ << '[';
    // TODO(1151236): show process id, thread id, timestamp and so on
    // if needed.
    if (severity_ >= 0) {
      stream_ << log_severity_name(severity_);
    } else {
      stream_ << "VERBOSE" << -severity_;
    }
    stream_ << ":" << filename << "(" << line << ")] ";
  }
  message_start_ = stream_.str().length();
}

#if BUILDFLAG(IS_WIN)
// This has already been defined in the header, but defining it again as DWORD
// ensures that the type used in the header is equivalent to DWORD. If not,
// the redefinition is a compile error.
typedef DWORD SystemErrorCode;
#endif

SystemErrorCode GetLastSystemErrorCode() {
#if BUILDFLAG(IS_WIN)
  return ::GetLastError();
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return errno;
#endif
}

PA_COMPONENT_EXPORT(PARTITION_ALLOC)
std::string SystemErrorCodeToString(SystemErrorCode error_code) {
#if BUILDFLAG(IS_WIN)
  const int kErrorMessageBufferSize = 256;
  char msgbuf[kErrorMessageBufferSize];
  DWORD flags = FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
  DWORD len = FormatMessageA(flags, nullptr, error_code, 0, msgbuf,
                             std::size(msgbuf), nullptr);
  if (len) {
    // Messages returned by system end with line breaks.
    std::string message(msgbuf);
    size_t whitespace_pos = message.find_last_not_of("\n\r ");
    if (whitespace_pos != std::string::npos)
      message.erase(whitespace_pos + 1);
    return message + base::TruncatingStringPrintf(" (0x%lX)", error_code);
  }
  return base::TruncatingStringPrintf(
      "Error (0x%lX) while retrieving error. (0x%lX)", GetLastError(),
      error_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  return base::safe_strerror(error_code) +
         base::TruncatingStringPrintf(" (%d)", error_code);
#endif  // BUILDFLAG(IS_WIN)
}

#if BUILDFLAG(IS_WIN)
Win32ErrorLogMessage::Win32ErrorLogMessage(const char* file,
                                           int line,
                                           LogSeverity severity,
                                           SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

Win32ErrorLogMessage::~Win32ErrorLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  DWORD last_error = err_;
  base::debug::Alias(&last_error);
}
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
ErrnoLogMessage::ErrnoLogMessage(const char* file,
                                 int line,
                                 LogSeverity severity,
                                 SystemErrorCode err)
    : LogMessage(file, line, severity), err_(err) {}

ErrnoLogMessage::~ErrnoLogMessage() {
  stream() << ": " << SystemErrorCodeToString(err_);
  // We're about to crash (CHECK). Put |err_| on the stack (by placing it in a
  // field) and use Alias in hopes that it makes it into crash dumps.
  int last_error = err_;
  base::debug::Alias(&last_error);
}
#endif  // BUILDFLAG(IS_WIN)

void RawLog(int level, const char* message) {
  if (level >= g_min_log_level && message) {
#if !BUILDFLAG(IS_WIN)
    const size_t message_len = strlen(message);
#else   // !BUILDFLAG(IS_WIN)
    const size_t message_len = ::lstrlenA(message);
#endif  // !BUILDFLAG(IS_WIN)
    WriteToStderr(message, message_len);

    if (message_len > 0 && message[message_len - 1] != '\n') {
      WriteToStderr("\n", 1);
    }
  }

  if (level == LOGGING_FATAL)
    PA_IMMEDIATE_CRASH();
}

// This was defined at the beginning of this file.
#undef write

}  // namespace partition_alloc::internal::logging
