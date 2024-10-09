// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"

#include <optional>

#include "base/check_op.h"
#include "base/check_version_internal.h"
#include "base/debug/alias.h"
#include "base/debug/dump_without_crashing.h"
#include "base/logging.h"
#include "base/thread_annotations.h"
#include "base/types/cxx23_to_underlying.h"
#include "build/build_config.h"

#if BUILDFLAG(IS_NACL)
// Forward declaring this ptr for code simplicity below, we'll never dereference
// it under NaCl.
namespace base::debug {
class CrashKeyString;
}  // namespace base::debug
#else
#include "base/debug/crash_logging.h"
#endif  // !BUILDFLAG(IS_NACL)

namespace logging {

namespace {

LogSeverity GetDumpSeverity() {
#if defined(OFFICIAL_BUILD)
  return DCHECK_IS_ON() ? LOGGING_DCHECK : LOGGING_ERROR;
#else
  // Crash outside official builds (outside user-facing builds) to detect
  // invariant violations early in release-build testing like fuzzing, etc.
  // These should eventually be migrated to fatal CHECKs.
  return LOGGING_FATAL;
#endif
}

LogSeverity GetNotFatalUntilSeverity(base::NotFatalUntil fatal_milestone) {
  if (fatal_milestone != base::NotFatalUntil::NoSpecifiedMilestoneInternal &&
      base::to_underlying(fatal_milestone) <= BASE_CHECK_VERSION_INTERNAL) {
    return LOGGING_FATAL;
  }
  return GetDumpSeverity();
}

LogSeverity GetCheckSeverity(base::NotFatalUntil fatal_milestone) {
  // CHECKs are fatal unless `fatal_milestone` overrides it.
  if (fatal_milestone == base::NotFatalUntil::NoSpecifiedMilestoneInternal) {
    return LOGGING_FATAL;
  }
  return GetNotFatalUntilSeverity(fatal_milestone);
}

base::debug::CrashKeyString* GetNotReachedCrashKey() {
#if BUILDFLAG(IS_NACL)
  return nullptr;
#else
  static auto* const key = ::base::debug::AllocateCrashKeyString(
      "Logging-NOTREACHED_MESSAGE", base::debug::CrashKeySize::Size1024);
  return key;
#endif  // BUILDFLAG(IS_NACL)
}

base::debug::CrashKeyString* GetDCheckCrashKey() {
#if BUILDFLAG(IS_NACL)
  return nullptr;
#else
  static auto* const key = ::base::debug::AllocateCrashKeyString(
      "Logging-DCHECK_MESSAGE", base::debug::CrashKeySize::Size1024);
  return key;
#endif  // BUILDFLAG(IS_NACL)
}

base::debug::CrashKeyString* GetDumpWillBeCheckCrashKey() {
#if BUILDFLAG(IS_NACL)
  return nullptr;
#else
  static auto* const key = ::base::debug::AllocateCrashKeyString(
      "Logging-DUMP_WILL_BE_CHECK_MESSAGE",
      base::debug::CrashKeySize::Size1024);
  return key;
#endif  // BUILDFLAG(IS_NACL)
}

#if !BUILDFLAG(IS_NACL)
base::debug::CrashKeyString* GetFatalMilestoneCrashKey() {
  static auto* const key = ::base::debug::AllocateCrashKeyString(
      "Logging-FATAL_MILESTONE", base::debug::CrashKeySize::Size32);
  return key;
}
#endif  // BUILDFLAG(IS_NACL)

void MaybeSetFatalMilestoneCrashKey(base::NotFatalUntil fatal_milestone) {
#if !BUILDFLAG(IS_NACL)
  if (fatal_milestone == base::NotFatalUntil::NoSpecifiedMilestoneInternal) {
    return;
  }
  base::debug::SetCrashKeyString(
      GetFatalMilestoneCrashKey(),
      base::NumberToString(base::to_underlying(fatal_milestone)));
#endif  // BUILDFLAG(IS_NACL)
}

void DumpWithoutCrashing(base::debug::CrashKeyString* message_key,
                         const logging::LogMessage* log_message,
                         const base::Location& location,
                         base::NotFatalUntil fatal_milestone) {
  const std::string crash_string = log_message->BuildCrashString();
#if !BUILDFLAG(IS_NACL)
  base::debug::ScopedCrashKeyString scoped_message_key(message_key,
                                                       crash_string);

  MaybeSetFatalMilestoneCrashKey(fatal_milestone);
#endif  // !BUILDFLAG(IS_NACL)
  // Copy the crash message to stack memory to make sure it can be recovered in
  // crash dumps. This is easier to recover in minidumps than crash keys during
  // local debugging.
  DEBUG_ALIAS_FOR_CSTR(log_message_str, crash_string.c_str(), 1024);

  // Report from the same location at most once every 30 days (unless the
  // process has died). This attempts to prevent us from flooding ourselves with
  // repeat reports for the same bug.
  base::debug::DumpWithoutCrashing(location, base::Days(30));

#if !BUILDFLAG(IS_NACL)
  base::debug::ClearCrashKeyString(GetFatalMilestoneCrashKey());
#endif  // !BUILDFLAG(IS_NACL)
}

void HandleCheckErrorLogMessage(base::debug::CrashKeyString* message_key,
                                const logging::LogMessage* log_message,
                                const base::Location& location,
                                base::NotFatalUntil fatal_milestone) {
  if (log_message->severity() == logging::LOGGING_FATAL) {
    // Set NotFatalUntil key if applicable for when we die in ~LogMessage.
    MaybeSetFatalMilestoneCrashKey(fatal_milestone);
  } else {
    DumpWithoutCrashing(message_key, log_message, location, fatal_milestone);
  }
}

class NotReachedLogMessage : public LogMessage {
 public:
  NotReachedLogMessage(const base::Location& location,
                       LogSeverity severity,
                       base::NotFatalUntil fatal_milestone)
      : LogMessage(location.file_name(), location.line_number(), severity),
        location_(location),
        fatal_milestone_(fatal_milestone) {}
  ~NotReachedLogMessage() override {
    HandleCheckErrorLogMessage(GetNotReachedCrashKey(), this, location_,
                               fatal_milestone_);
  }

 private:
  const base::Location location_;
  const base::NotFatalUntil fatal_milestone_;
};

class DCheckLogMessage : public LogMessage {
 public:
  DCheckLogMessage(const base::Location& location)
      : LogMessage(location.file_name(),
                   location.line_number(),
                   LOGGING_DCHECK),
        location_(location) {}
  ~DCheckLogMessage() override {
    HandleCheckErrorLogMessage(
        GetDCheckCrashKey(), this, location_,
        base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  }

 private:
  const base::Location location_;
};

class CheckLogMessage : public LogMessage {
 public:
  CheckLogMessage(const base::Location& location,
                  LogSeverity severity,
                  base::NotFatalUntil fatal_milestone)
      : LogMessage(location.file_name(), location.line_number(), severity),
        location_(location),
        fatal_milestone_(fatal_milestone) {}
  ~CheckLogMessage() override {
    HandleCheckErrorLogMessage(GetDumpWillBeCheckCrashKey(), this, location_,
                               fatal_milestone_);
  }

 private:
  const base::Location location_;
  const base::NotFatalUntil fatal_milestone_;
};

#if BUILDFLAG(IS_WIN)
class DCheckWin32ErrorLogMessage : public Win32ErrorLogMessage {
 public:
  DCheckWin32ErrorLogMessage(const base::Location& location,
                             SystemErrorCode err)
      : Win32ErrorLogMessage(location.file_name(),
                             location.line_number(),
                             LOGGING_DCHECK,
                             err),
        location_(location) {}
  ~DCheckWin32ErrorLogMessage() override {
    HandleCheckErrorLogMessage(
        GetDCheckCrashKey(), this, location_,
        base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  }

 private:
  const base::Location location_;
};
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
class DCheckErrnoLogMessage : public ErrnoLogMessage {
 public:
  DCheckErrnoLogMessage(const base::Location& location, SystemErrorCode err)
      : ErrnoLogMessage(location.file_name(),
                        location.line_number(),
                        LOGGING_DCHECK,
                        err),
        location_(location) {}
  ~DCheckErrnoLogMessage() override {
    HandleCheckErrorLogMessage(
        GetDCheckCrashKey(), this, location_,
        base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  }

 private:
  const base::Location location_;
};
#endif  // BUILDFLAG(IS_WIN)

}  // namespace

CheckError CheckError::Check(const char* condition,
                             base::NotFatalUntil fatal_milestone,
                             const base::Location& location) {
  auto* const log_message = new CheckLogMessage(
      location, GetCheckSeverity(fatal_milestone), fatal_milestone);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::CheckOp(char* log_message_str,
                               base::NotFatalUntil fatal_milestone,
                               const base::Location& location) {
  auto* const log_message = new CheckLogMessage(
      location, GetCheckSeverity(fatal_milestone), fatal_milestone);
  log_message->stream() << log_message_str;
  free(log_message_str);
  return CheckError(log_message);
}

CheckError CheckError::DCheck(const char* condition,
                              const base::Location& location) {
  auto* const log_message = new DCheckLogMessage(location);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DCheckOp(char* log_message_str,
                                const base::Location& location) {
  auto* const log_message = new DCheckLogMessage(location);
  log_message->stream() << log_message_str;
  free(log_message_str);
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeCheck(const char* condition,
                                       const base::Location& location) {
  auto* const log_message =
      new CheckLogMessage(location, GetDumpSeverity(),
                          base::NotFatalUntil::NoSpecifiedMilestoneInternal);
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeCheckOp(char* log_message_str,
                                         const base::Location& location) {
  auto* const log_message =
      new CheckLogMessage(location, GetDumpSeverity(),
                          base::NotFatalUntil::NoSpecifiedMilestoneInternal);
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
  auto* const log_message = new DCheckWin32ErrorLogMessage(location, err_code);
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  auto* const log_message = new DCheckErrnoLogMessage(location, err_code);
#endif
  log_message->stream() << "Check failed: " << condition << ". ";
  return CheckError(log_message);
}

CheckError CheckError::DumpWillBeNotReachedNoreturn(
    const base::Location& location) {
  auto* const log_message = new NotReachedLogMessage(
      location, GetDumpSeverity(),
      base::NotFatalUntil::NoSpecifiedMilestoneInternal);
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
  // TODO(crbug.com/40254046): Consider splitting out CHECK from DCHECK so that
  // the destructor can be marked [[noreturn]] and we don't need to check
  // severity in the destructor.
  const bool is_fatal = log_message_->severity() == LOGGING_FATAL;
  // Note: This function ends up in crash stack traces. If its full name
  // changes, the crash server's magic signature logic needs to be updated.
  // See cl/306632920.

  // Reset before `ImmediateCrash()` to ensure the message is flushed.
  log_message_.reset();

  // Make sure we crash even if LOG(FATAL) has been overridden.
  // TODO(crbug.com/40254046): Remove severity checking in the destructor when
  // LOG(FATAL) is [[noreturn]] and can't be overridden.
  if (is_fatal) {
    base::ImmediateCrash();
  }
}

CheckError::CheckError(LogMessage* log_message) : log_message_(log_message) {}

NotReachedError NotReachedError::NotReached(base::NotFatalUntil fatal_milestone,
                                            const base::Location& location) {
  auto* const log_message = new NotReachedLogMessage(
      location, GetCheckSeverity(fatal_milestone), fatal_milestone);

  // TODO(pbos): Consider a better message for NotReached(), this is here to
  // match existing behavior + test expectations.
  log_message->stream() << "Check failed: false. ";
  return NotReachedError(log_message);
}

void NotReachedError::TriggerNotReached() {
  // This triggers a NOTREACHED_IN_MIGRATION() error as the returned
  // NotReachedError goes out of scope.
  NotReached()
      << "NOTREACHED log messages are omitted in official builds. Sorry!";
}

NotReachedError::~NotReachedError() = default;

NotReachedNoreturnError::NotReachedNoreturnError(const base::Location& location)
    : CheckError([location] {
        auto* const log_message = new NotReachedLogMessage(
            location, LOGGING_FATAL,
            base::NotFatalUntil::NoSpecifiedMilestoneInternal);
        log_message->stream() << "NOTREACHED hit. ";
        return log_message;
      }()) {}

// Note: This function ends up in crash stack traces. If its full name changes,
// the crash server's magic signature logic needs to be updated. See
// cl/306632920.
NotReachedNoreturnError::~NotReachedNoreturnError() {
  // Reset before `ImmediateCrash()` to ensure the message is flushed.
  log_message_.reset();

  // Make sure we die if we haven't.
  // TODO(crbug.com/40254046): Replace this with NOTREACHED() once LOG(FATAL) is
  // [[noreturn]].
  base::ImmediateCrash();
}

void RawCheckFailure(const char* message) {
  RawLog(LOGGING_FATAL, message);
  __builtin_unreachable();
}

}  // namespace logging
