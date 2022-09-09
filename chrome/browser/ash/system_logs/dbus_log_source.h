// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_

#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches memory usage details.
class DBusLogSource : public SystemLogsSource {
 public:
  DBusLogSource();

  DBusLogSource(const DBusLogSource&) = delete;
  DBusLogSource& operator=(const DBusLogSource&) = delete;

  ~DBusLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_
