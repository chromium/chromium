// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/check.h"

// check.h is a widely included header and its size has significant impact on
// build time. Try not to raise this limit unless absolutely necessary. See
// https://chromium.googlesource.com/chromium/src/+/HEAD/docs/wmax_tokens.md
#ifndef NACL_TC_REV
#pragma clang max_tokens_here 17000
#endif

#include "base/check_op.h"
#include "base/logging.h"
#include "build/build_config.h"

namespace logging {

CheckError CheckError::Check(const char* file,
                             int line,
                             const char* condition) {
  CheckError check_error(new LogMessage(file, line, LOGGING_FATAL));
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::CheckOp(const char* file,
                               int line,
                               CheckOpResult* check_op_result) {
  CheckError check_error(new LogMessage(file, line, LOGGING_FATAL));
  check_error.stream() << "Check failed: " << check_op_result->message_;
  free(check_op_result->message_);
  check_op_result->message_ = nullptr;
  return check_error;
}

CheckError CheckError::DCheck(const char* file,
                              int line,
                              const char* condition) {
  CheckError check_error(new LogMessage(file, line, LOGGING_DCHECK));
  check_error.stream() << "Check failed: " << condition << ". ";
  return check_error;
}

CheckError CheckError::DCheckOp(const char* file,
                                int line,
                                CheckOpResult* check_op_result) {
  CheckError check_error(new LogMessage(file, line, LOGGING_DCHECK));
  check_error.stream() << "Check failed: " << check_op_result->message_;
  free(check_op_result->message_);
  check_op_result->message_ = nullptr;
  return check_error;
}

CheckError CheckError::PCheck(const char* file,
                              int line,
                              const char* condition) {
  SystemErrorCode err_code = logging::GetLastSystemErrorCode();
#if defined(OS_WIN)
  CheckError check_error(
      new Win32ErrorLogMessage(file, line, LOGGING_FATAL, err_code));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
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
#if defined(OS_WIN)
  CheckError check_error(
      new Win32ErrorLogMessage(file, line, LOGGING_DCHECK, err_code));
#elif defined(OS_POSIX) || defined(OS_FUCHSIA)
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

}  // namespace logging
