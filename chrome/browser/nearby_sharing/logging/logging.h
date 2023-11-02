// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEARBY_SHARING_LOGGING_LOGGING_H_
#define CHROME_BROWSER_NEARBY_SHARING_LOGGING_LOGGING_H_

#include <sstream>

#include "base/logging.h"

// Use the NS_LOG() macro for all logging related to Nearby Sharing so the debug
// page can reflect all logs related to this feature in the internal debug WebUI
// (chrome://nearby-internals).
#define NS_LOG(severity) \
  ScopedLogMessage(__FILE__, __LINE__, logging::LOG_##severity).stream()

// An intermediate object used by the NS_LOG macro, wrapping a
// logging::LogMessage instance. When this object is destroyed, the message will
// be logged with the standard logging system and also added to Nearby Sharing
// specific log buffer.
class ScopedLogMessage {
 public:
  ScopedLogMessage(const char* file, int line, logging::LogSeverity severity);
  ScopedLogMessage(const ScopedLogMessage&) = delete;
  ScopedLogMessage& operator=(const ScopedLogMessage&) = delete;
  ~ScopedLogMessage();

  std::ostream& stream() { return stream_; }

 private:
  const char* file_;
  int line_;
  logging::LogSeverity severity_;
  std::ostringstream stream_;
};

#endif  // CHROME_BROWSER_NEARBY_SHARING_LOGGING_LOGGING_H_
