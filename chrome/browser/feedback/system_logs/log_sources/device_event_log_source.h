// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_DEVICE_EVENT_LOG_SOURCE_H_
#define CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_DEVICE_EVENT_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches entries for 'network_event_log' and 'device_event_log'.
class DeviceEventLogSource : public SystemLogsSource {
 public:
  DeviceEventLogSource();

  DeviceEventLogSource(const DeviceEventLogSource&) = delete;
  DeviceEventLogSource& operator=(const DeviceEventLogSource&) = delete;

  ~DeviceEventLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_FEEDBACK_SYSTEM_LOGS_LOG_SOURCES_DEVICE_EVENT_LOG_SOURCE_H_
