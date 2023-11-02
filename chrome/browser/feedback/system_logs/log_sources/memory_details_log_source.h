// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches memory usage details.
class MemoryDetailsLogSource : public SystemLogsSource {
 public:
  MemoryDetailsLogSource();

  MemoryDetailsLogSource(const MemoryDetailsLogSource&) = delete;
  MemoryDetailsLogSource& operator=(const MemoryDetailsLogSource&) = delete;

  ~MemoryDetailsLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_MEMORY_DETAILS_LOG_SOURCE_H_
