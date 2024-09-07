// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_
#define CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_

#include <vector>

#include "base/location.h"
#include "base/logging.h"
#include "components/drive/event_logger.h"

namespace sync_file_system {
// Originally wanted to use 'logging' here, but it conflicts with
// base/logging.h, and breaks DCHECK() and friends.
namespace util {

// Resets the log.
void ClearLog();

// Logs a message using printf format.
// This function can be called from any thread.
PRINTF_FORMAT(3, 4)
void Log(logging::LogSeverity level,
         const base::Location& location,
         const char* format,
         ...);

// Returns the log history.
// This function can be called from any thread.
std::vector<drive::EventLogger::Event> GetLogHistory();

}  // namespace util
}  // namespace sync_file_system

#endif  // CHROME_BROWSER_SYNC_FILE_SYSTEM_LOGGER_H_
