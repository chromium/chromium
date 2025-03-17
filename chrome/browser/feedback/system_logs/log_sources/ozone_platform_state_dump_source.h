// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_PLATFORM_STATE_DUMP_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_PLATFORM_STATE_DUMP_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches Ozone state dump.
class OzonePlatformStateDumpSource : public SystemLogsSource {
 public:
  OzonePlatformStateDumpSource();
  OzonePlatformStateDumpSource(const OzonePlatformStateDumpSource&) = delete;
  OzonePlatformStateDumpSource& operator=(const OzonePlatformStateDumpSource&) =
      delete;
  ~OzonePlatformStateDumpSource() override = default;

  // SystemLogsSource:
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_OZONE_PLATFORM_STATE_DUMP_SOURCE_H_
