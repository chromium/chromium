// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/logger.h"

#include "base/files/file_util.h"
#include "base/lazy_instance.h"
#include "base/location.h"
#include "base/notreached.h"
#include "base/strings/stringprintf.h"
#include "components/drive/event_logger.h"

namespace sync_file_system {
namespace util {
namespace {

static base::LazyInstance<drive::EventLogger>::DestructorAtExit g_logger =
    LAZY_INSTANCE_INITIALIZER;

const char* LogSeverityToString(logging::LogSeverity level) {
  switch (level) {
    case logging::LOGGING_ERROR:
      return "ERROR";
    case logging::LOGGING_WARNING:
      return "WARNING";
    case logging::LOGGING_INFO:
      return "INFO";
    case logging::LOGGING_VERBOSE:
      return "VERBOSE";
  }

  NOTREACHED_IN_MIGRATION();
  return "Unknown Log Severity";
}

}  // namespace

void ClearLog() {
  g_logger.Pointer()->SetHistorySize(drive::kDefaultHistorySize);
}

void Log(logging::LogSeverity severity,
         const base::Location& location,
         const char* format,
         ...) {
  std::string what;

  va_list args;
  va_start(args, format);
  base::StringAppendV(&what, format, args);
  va_end(args);

  // Log to WebUI regardless of LogSeverity (e.g. ignores command line flags).
  // On thread-safety: LazyInstance guarantees thread-safety for the object
  // creation. EventLogger::Log() internally maintains the lock.
  drive::EventLogger* ptr = g_logger.Pointer();
  ptr->LogRawString(severity, base::StringPrintf("[%s] %s",
                                                 LogSeverityToString(severity),
                                                 what.c_str()));

  // Log to console if the severity is at or above the min level.
  // LOGGING_VERBOSE logs are also output if the verbosity of this module
  // (sync_file_system/logger) is >= 1.
  // TODO(kinuko,calvinlo): Reconsider this logging hack, it's not recommended
  // to directly use LogMessage.
  if (severity < logging::GetMinLogLevel() && !VLOG_IS_ON(1))
    return;
  logging::LogMessage(location.file_name(), location.line_number(), severity)
      .stream() << what;
}

std::vector<drive::EventLogger::Event> GetLogHistory() {
  drive::EventLogger* ptr = g_logger.Pointer();
  return ptr->GetHistory();
}

}  // namespace util
}  // namespace sync_file_system
