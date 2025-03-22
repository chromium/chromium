// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_QUICK_PAIR_COMMON_LOGGING_H_
#define ASH_QUICK_PAIR_COMMON_LOGGING_H_

#include <sstream>
#include <string_view>

#include "base/component_export.h"
#include "base/logging.h"

namespace ash {
namespace quick_pair {

// Use the QP_LOG() macro for all logging related to Quick Pair implementations
// (e.g. Fast Pair), so the system is aware of all logs related to this feature.
// We display these logs in the debug WebUI (chrome://nearby-internals).
//
// QP_LOG() has the same interface as the standard LOG() macro and also creates
// a normal log message of the same severity.
// Examples:
//   QP_LOG(INFO) << "Waiting for " << x << " pending requests.";
//   QP_LOG(ERROR) << "Request failed: " << error_string;
#define QP_LOG(severity)                                         \
  ash::quick_pair::ScopedLogMessage(                             \
      std::string_view(__FILE__, std::size(__FILE__)), __LINE__, \
      logging::LOGGING_##severity)                               \
      .stream()

// Disables all logging while in scope. Intended to be called only from test
// code, to clean up test output.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) ScopedDisableLoggingForTesting {
 public:
  ScopedDisableLoggingForTesting();
  ~ScopedDisableLoggingForTesting();
};

// An intermediate object used by the QP_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system and also added to Proximity Auth
// specific log buffer.  You should use the QP_LOG() macro instead of this class
// directly.
class COMPONENT_EXPORT(QUICK_PAIR_COMMON) ScopedLogMessage {
 public:
  ScopedLogMessage(std::string_view file,
                   int line,
                   logging::LogSeverity severity);
  ScopedLogMessage(const ScopedLogMessage&) = delete;
  ScopedLogMessage& operator=(const ScopedLogMessage&) = delete;
  ~ScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const std::string_view file_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

}  // namespace quick_pair
}  // namespace ash

#endif  // ASH_QUICK_PAIR_COMMON_LOGGING_H_
