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

namespace logging {

namespace {

void DumpWithoutCrashing(LogMessage* log_message,
                         const base::Location& location) {
  // Copy the LogMessage message to stack memory to make sure it can be
  // recovered in crash dumps. This is easier to recover in minidumps than crash
  // keys during local debugging.
  DEBUG_ALIAS_FOR_CSTR(log_message_str, log_message->BuildCrashString().c_str(),
                       1024);

  // Report from the same location at most once every 30 days (unless the
  // process has died). This attempts to prevent us from flooding ourselves with
  // repeat reports for the same bug.
  base::debug::DumpWithoutCrashing(location, base::Days(30));
}

void NotReachedDumpWithoutCrashing(LogMessage* log_message,
                                   const base::Location& location) {
#if !BUILDFLAG(IS_NACL)
  SCOPED_CRASH_KEY_STRING1024("Logging", "NOTREACHED_MESSAGE",
                              log_message->BuildCrashString());
#endif  // !BUILDFLAG(IS_NACL)
  DumpWithoutCrashing(log_message, location);
}

void DCheckDumpWithoutCrashing(LogMessage* log_message,
                               const base::Location& location) {
#if !BUILDFLAG(IS_NACL)
  SCOPED_CRASH_KEY_STRING1024("Logging", "DCHECK_MESSAGE",
                              log_message->BuildCrashString());
#endif  // !BUILDFLAG(IS_NACL)
  DumpWithoutCrashing(log_message, location);
}

void DumpWillBeCheckDumpWithoutCrashing(LogMessage* log_message,
                                        const base::Location& location) {
#if !BUILDFLAG(IS_NACL)
  SCOPED_CRASH_KEY_STRING1024("Logging", "DUMP_WILL_BE_CHECK_MESSAGE",
                              log_message->BuildCrashString());
#endif  // !BUILDFLAG(IS_NACL)
  DumpWithoutCrashing(log_message, location);
}

class NotReachedLogMessage : public LogMessage {
 public:
  NotReachedLogMessage(const base::Location& location, LogSeverity severity)
      : LogMessage(location.file_name(), location.line_number(), severity),
        location_(location) {}
  ~NotReachedLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      NotReachedDumpWithoutCrashing(this, location_);
    }
  }

 private:
  const base::Location location_;
};

class DCheckLogMessage : public LogMessage {
 public:
  using LogMessage::LogMessage;
  DCheckLogMessage(const base::Location& location, LogSeverity severity)
      : LogMessage(location.file_name(), location.line_number(), severity),
        location_(location) {}
  ~DCheckLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpWithoutCrashing(this, location_);
    }
  }

 private:
  const base::Location location_;
};

class DumpWillBeCheckLogMessage : public LogMessage {
 public:
  using LogMessage::LogMessage;
  DumpWillBeCheckLogMessage(const base::Location& location,
                            LogSeverity severity)
      : LogMessage(location.file_name(), location.line_number(), severity),
        location_(location) {}
  ~DumpWillBeCheckLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DumpWillBeCheckDumpWithoutCrashing(this, location_);
    }
  }

 private:
  const base::Location location_;
};

#if BUILDFLAG(IS_WIN)
class DCheckWin32ErrorLogMessage : public Win32ErrorLogMessage {
 public:
  DCheckWin32ErrorLogMessage(const base::Location& location,
                             LogSeverity severity,
                             SystemErrorCode err)
      : Win32ErrorLogMessage(location.file_name(),
                             location.line_number(),
                             severity,
                             err),
        location_(location) {}
  ~DCheckWin32ErrorLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpWithoutCrashing(this, location_);
    }
  }

 private:
  const base::Location location_;
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
class DCheckErrnoLogMessage : public ErrnoLogMessage {
 public:
  DCheckErrnoLogMessage(const base::Location& location,
                        LogSeverity severity,
                        SystemErrorCode err)
      : ErrnoLogMessage(location.file_name(),
                        location.line_number(),
                        severity,
                        err),
        location_(location) {}
  ~DCheckErrnoLogMessage() override {
    if (severity() != logging::LOGGING_FATAL) {
      DCheckDumpWithoutCrashing(this, location_);
    }
  }

 private:
  const base::Location location_;
};
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

CheckError CheckError::Check(const char* condition,
                             const base::Location& location) {
  auto* const log_message = new LogMessage(
      location.file_name(), location.line_number(), LOGGING_FATAL);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::CheckOp(char* log_message_str,
                               const base::Location& location) {
  auto* const log_message = new LogMessage(
      location.file_name(), location.line_number(), LOGGING_FATAL);
  log_message->stream() << log_message_str;
  free(log_message_str);
  return CheckError(log_message);
}

CheckError CheckError::DCheck(const char* condition,
                              const base::Location& location) {
  auto* const log_message = new DCheckLogMessage(location, LOGGING_DCHECK);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DCheckOp(char* log_message_str,
                                const base::Location& location) {
  auto* const log_message = new DCheckLogMessage(
      location.file_name(), location.line_number(), LOGGING_DCHECK);
  log_message->stream() << log_message_str;
  free(log_message_str);
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeCheck(const char* condition,
                                       const base::Location& location) {
  auto* const log_message = new DumpWillBeCheckLogMessage(
      location, DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeCheckOp(char* log_message_str,
                                         const base::Location& location) {
  auto* const log_message = new DumpWillBeCheckLogMessage(
      location, DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR);
  log_message->stream() << log_message_str;
  free(log_message_str);
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const char* condition,
                              const base::Location& location) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message = new Win32ErrorLogMessage(
      location.file_name(), location.line_number(), LOGGING_FATAL, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message = new ErrnoLogMessage(
      location.file_name(), location.line_number(), LOGGING_FATAL, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::PCheck(const base::Location& location) {
  return PCheck("", location);
}

CheckError CheckError::DPCheck(const char* condition,
                               const base::Location& location) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  auto* const log_message =
      new DCheckWin32ErrorLogMessage(location, LOGGING_DCHECK, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message =
      new DCheckErrnoLogMessage(location, LOGGING_DCHECK, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeNotReachedNoreturn(
    const base::Location& location) {
  auto* const log_message = new DumpWillBeCheckLogMessage(
      location, DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR);
  log_message->stream() << "NOTREACHED hit. ";
  return CheckError(log_message);
}

CheckError CheckError::NotImplemented(const char* function,
                                      const base::Location& location) {
  auto* const log_message = new LogMessage(
      location.file_name(), location.line_number(), LOGGING_ERROR);
  log_message->stream() << "Not implemented reached in " << function;
  return CheckError(log_message);
}

std::ostream& CheckError::stream() {
  return log_message_->stream();
}

CheckError::~CheckError() {
  // TODO(crbug.com/1409729): Consider splitting out CHECK from DCHECK so that
  // the destructor can be marked [[noreturn]] and we don't need to check
  // severity in the destructor.
  const bool is_fatal = log_message_->severity() == LOGGING_FATAL;
  // Note: This function ends up in crash stack traces. If its full name
  // changes, the crash server's magic signature logic needs to be updated.
  // See cl/306632920.
  delete log_message_;

  // Make sure we crash even if LOG(FATAL) has been overridden.
  // TODO(crbug.com/1409729): Remove severity checking in the destructor when
  // LOG(FATAL) is [[noreturn]] and can't be overridden.
  if (is_fatal) {
    base::ImmediateCrash();
  }
}

NotReachedError NotReachedError::NotReached(const base::Location& location) {
  // Outside DCHECK builds NOTREACHED() should not be FATAL. For now.
  const LogSeverity severity = DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR;
  auto* const log_message = new NotReachedLogMessage(location, severity);

  // TODO(pbos): Consider a better message for NotReached(), this is here to
  // match existing behavior + test expectations.
  log_message->stream() << "Check failed: false. ";
  return NotReachedError(log_message);
}

void NotReachedError::TriggerNotReached() {
  // This triggers a NOTREACHED() error as the returned NotReachedError goes out
  // of scope.
  NotReached()
      << "NOTREACHED log messages are omitted in official builds. Sorry!";
}

NotReachedError::~NotReachedError() = default;

NotReachedNoreturnError::NotReachedNoreturnError(const base::Location& location)
    : CheckError([location]() {
        auto* const log_message = new LogMessage(
            location.file_name(), location.line_number(), LOGGING_FATAL);
        log_message->stream() << "NOTREACHED hit. ";
        return log_message;
      }()) {}

// Note: This function ends up in crash stack traces. If its full name changes,
// the crash server's magic signature logic needs to be updated. See
// cl/306632920.
NotReachedNoreturnError::~NotReachedNoreturnError() {
  delete log_message_;

  // Make sure we die if we haven't.
  // TODO(crbug.com/1409729): Replace this with NOTREACHED_NORETURN() once
  // LOG(FATAL) is [[noreturn]].
  base::ImmediateCrash();
}

void RawCheckFailure(const char* message) {
  RawLog(LOGGING_FATAL, message);
  __builtin_unreachable();
}

}  // namespace logging
