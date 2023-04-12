// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feedback/system_logs/log_sources/device_event_log_source.h"

#include "components/device_event_log/device_event_log.h"
#include "content/public/browser/browser_thread.h"

namespace system_logs {

const char kNetworkEventLogEntry[] = "network_event_log";
const char kDeviceEventLogEntry[] = "device_event_log";

DeviceEventLogSource::DeviceEventLogSource()
    : SystemLogsSource("DeviceEvent") {}

DeviceEventLogSource::~DeviceEventLogSource() {}

void DeviceEventLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  const int kMaxDeviceEventsForAboutSystem = 4000;
  (*response)[kNetworkEventLogEntry] = device_event_log::GetAsString(
      device_event_log::OLDEST_FIRST, "unixtime,file,level", "network",
      device_event_log::LOG_LEVEL_EVENT, kMaxDeviceEventsForAboutSystem);
  (*response)[kDeviceEventLogEntry] = device_event_log::GetAsString(
      device_event_log::OLDEST_FIRST, "unixtime,file,type,level", "non-network",
      device_event_log::LOG_LEVEL_EVENT, kMaxDeviceEventsForAboutSystem);
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
