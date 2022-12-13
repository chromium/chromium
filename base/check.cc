// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"

#include "base/check_op.h"
#include "base/debug/alias.h"
#include "base/debug/debugging_buildflags.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/thread_annotations.h"
#include "build/build_config.h"

#if !BUILDFLAG(IS_NACL)
#include "base/debug/crash_logging.h"
#endif  // !BUILDFLAG(IS_NACL)

#include <atomic>

namespace logging {

namespace {

// DCHECK_IS_CONFIGURABLE and ENABLE_LOG_ERROR_NOT_REACHED are both interested
// in non-FATAL DCHECK()/NOTREACHED() reports.
#if BUILDFLAG(DCHECK_IS_CONFIGURABLE) || BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)
void DumpOnceWithoutCrashing(LogMessage* log_message) {
  // Best-effort gate to prevent multiple DCHECKs from being dumped. This will
  // race if multiple threads DCHECK at the same time, but we'll eventually stop
  // reporting and at most report once per thread.
  static std::atomic<bool> has_dumped = false;
  if (!has_dumped.load(std::memory_order_relaxed)) {
    // Copy the LogMessage message to stack memory to make sure it can be
    // recovered in crash dumps.
    // TODO(pbos): Do we need this for NACL builds or is the crash key set in
    // the caller sufficient?
    DEBUG_ALIAS_FOR_CSTR(log_message_str,
                         log_message->BuildCrashString().c_str(), 1024);

    // Note that dumping may fail if the crash handler hasn't been set yet. In
    // that case we want to try again on the next failing DCHECK.
    if (base::debug::DumpWithoutCrashingUnthrottled()) {
      has_dumped.store(true, std::memory_order_relaxed);
    }
  }
}

void NotReachedDumpOnceWithoutCrashing(LogMessage* log_message) {
#if !BUILDFLAG(IS_NACL)
  SCOPED_CRASH_KEY_STRING1024("Logging", "NOTREACHED_MESSAGE",
                              log_message->BuildCrashString());
#endif  // !BUILDFLAG(IS_NACL)
  DumpOnceWithoutCrashing(log_message);
}

class NotReachedLogMessage : public LogMessage {
 public:
  using LogMessage::LogMessage;
  ~NotReachedLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      NotReachedDumpOnceWithoutCrashing(this);
    }
  }
};
#else
using NotReachedLogMessage = LogMessage;
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE) ||
        // BUILDFLAG(ENABLE_LOG_ERROR_NOT_REACHED)

#if BUILDFLAG(DCHECK_IS_CONFIGURABLE)

void DCheckDumpOnceWithoutCrashing(LogMessage* log_message) {
#if !BUILDFLAG(IS_NACL)
  SCOPED_CRASH_KEY_STRING1024("Logging", "DCHECK_MESSAGE",
                              log_message->BuildCrashString());
#endif  // !BUILDFLAG(IS_NACL)
  DumpOnceWithoutCrashing(log_message);
}

class DCheckLogMessage : public LogMessage {
 public:
  using LogMessage::LogMessage;
  ~DCheckLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpOnceWithoutCrashing(this);
    }
  }
};

#if BUILDFLAG(IS_WIN)
class DCheckWin32ErrorLogMessage : public Win32ErrorLogMessage {
 public:
  using Win32ErrorLogMessage::Win32ErrorLogMessage;
  ~DCheckWin32ErrorLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpOnceWithoutCrashing(this);
    }
  }
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
class DCheckErrnoLogMessage : public ErrnoLogMessage {
 public:
  using ErrnoLogMessage::ErrnoLogMessage;
  ~DCheckErrnoLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpOnceWithoutCrashing(this);
    }
  }
};
#endif  // BUILDFLAG(IS_WIN)
#else
static_assert(logging::LOGGING_DCHECK == logging::LOGGING_FATAL);
using DCheckLogMessage = LogMessage;
#if BUILDFLAG(IS_WIN)
using DCheckWin32ErrorLogMessage = Win32ErrorLogMessage;
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
using DCheckErrnoLogMessage = ErrnoLogMessage;
#endif  // BUILDFLAG(IS_WIN)
#endif  // BUILDFLAG(DCHECK_IS_CONFIGURABLE)

}  // namespace

CheckError CheckError::Check(const char* file,
                             int line,
                             const char* condition) {
  auto* const log_message = new LogMessage(file, line, LOGGING_FATAL);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DCheck(const char* file,
                              int line,
                              const char* condition) {
  auto* const log_message = new DCheckLogMessage(file, line, LOGGING_DCHECK);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const char* file,
                              int line,
                              const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message =
      new Win32ErrorLogMessage(file, line, LOGGING_FATAL, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message =
      new ErrnoLogMessage(file, line, LOGGING_FATAL, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const char* file, int line) {
  return PCheck(file, line, "");
}

CheckError CheckError::DPCheck(const char* file,
                               int line,
                               const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message =
      new DCheckWin32ErrorLogMessage(file, line, LOGGING_DCHECK, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message =
      new DCheckErrnoLogMessage(file, line, LOGGING_DCHECK, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::NotImplemented(const char* file,
                                      int line,
                                      const char* function) {
  auto* const log_message = new LogMessage(file, line, LOGGING_ERROR);
  log_message->stream() << "Not implemented reached in " << function;
  return CheckError(log_message);
}

std::ostream& CheckError::stream() {
  return log_message_->stream();
}

CheckError::~CheckError() {
  // Note: This function ends up in crash stack traces. If its full name
  // changes, the crash server's magic signature logic needs to be updated.
  // See cl/306632920.
  delete log_message_;
}

NotReachedError NotReachedError::NotReached(const char* file, int line) {
  // Outside DCHECK builds NOTREACHED() should not be FATAL. For now.
  const LogSeverity severity = DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR;
  auto* const log_message = new NotReachedLogMessage(file, line, severity);

  // TODO(pbos): Consider a better message for NotReached(), this is here to
  // match existing behavior + test expectations.
  log_message->stream() << "Check failed: false. ";
  return NotReachedError(log_message);
}

NotReachedError::~NotReachedError() = default;

void RawCheck(const char* message) {
  RawLog(LOGGING_FATAL, message);
}

void RawError(const char* message) {
  RawLog(LOGGING_ERROR, message);
}

}  // namespace logging
