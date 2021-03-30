// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_
#define CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_

#include "base/macros.h"
#include "components/feedback/system_logs/system_logs_source.h"

namespace system_logs {

// Fetches memory usage details.
class DBusLogSource : public SystemLogsSource {
 public:
  DBusLogSource();
  ~DBusLogSource() override;

  // SystemLogsSource override.
  void Fetch(SysLogsSourceCallback request) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DBusLogSource);
};

}  // namespace system_logs

#endif  // CHROME_BROWSER_ASH_SYSTEM_LOGS_DBUS_LOG_SOURCE_H_
