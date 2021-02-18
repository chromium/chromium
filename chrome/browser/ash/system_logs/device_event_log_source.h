// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_DEVICE_EVENT_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_DEVICE_EVENT_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches entries for 'network_event_log' and 'device_event_log'.
class DeviceEventLogSource : public SystemLogsSource {
 public:
  DeviceEventLogSource();
  ~DeviceEventLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DeviceEventLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_DEVICE_EVENT_LOG_SOURCE_H_
