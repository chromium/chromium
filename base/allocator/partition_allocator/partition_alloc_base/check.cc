// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_alloc_base/check.h"

#include "base/allocator/partition_allocator/partition_alloc_base/logging.h"

namespace partition_alloc::internal::logging {

// TODO(1151236): Make CheckError not to allocate memory. So we can use
// CHECK() inside PartitionAllocator when PartitionAllocator-Everywhere is
// enabled. (Also need to modify LogMessage).
CheckError CheckError::Check(const char* file,
                             int line,
                             const char* condition) {
  CheckError check_error(new LogMessage(file, line, LOGGING_FATAL));
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::DCheck(const char* file,
                              int line,
                              const char* condition) {
  CheckError check_error(new LogMessage(file, line, LOGGING_DCHECK));
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::PCheck(const char* file,
                              int line,
                              const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  CheckError check_error(
      new Win32ErrorLogMessage(file, line, LOGGING_FATAL, err_code));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  CheckError check_error(
      new ErrnoLogMessage(file, line, LOGGING_FATAL, err_code));
#endif
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::PCheck(const char* file, int line) {
  return PCheck(file, line, "");
}

CheckError CheckError::DPCheck(const char* file,
                               int line,
                               const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if BUILDFLAG(IS_WIN)
  CheckError check_error(
      new Win32ErrorLogMessage(file, line, LOGGING_DCHECK, err_code));
#elif BUILDFLAG(IS_POSIX) || BUILDFLAG(IS_FUCHSIA)
  CheckError check_error(
      new ErrnoLogMessage(file, line, LOGGING_DCHECK, err_code));
#endif
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::NotImplemented(const char* file,
                                      int line,
                                      const char* function) {
  CheckError check_error(new LogMessage(file, line, LOGGING_ERROR));
  check_error.stream() << "Not implemented reached in " << function;
  return check_error;
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

CheckError::CheckError(LogMessage* log_message) : log_message_(log_message) {}

void RawCheck(const char* message) {
  RawLog(LOGGING_FATAL, message);
}

void RawError(const char* message) {
  RawLog(LOGGING_ERROR, message);
}

}  // namespace partition_alloc::internal::logging
