// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_logs/dbus_log_source.h"

#include <memory>

#include "content/public/browser/browser_thread.h"
#include "dbus/dbus_statistics.h"

const char kDBusLogEntryShort[] = "dbus_summary";
const char kDBusLogEntryLong[] = "dbus_details";

namespace system_logs {

DBusLogSource::DBusLogSource() : SystemLogsSource("DBus") {
}

DBusLogSource::~DBusLogSource() {
}

void DBusLogSource::Fetch(SysLogsSourceCallback callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  DCHECK(!callback.is_null());

  auto response = std::make_unique<SystemLogsResponse>();
  response->emplace(kDBusLogEntryShort, dbus::statistics::GetAsString(
                                            dbus::statistics::SHOW_INTERFACE,
                                            dbus::statistics::FORMAT_ALL));
  response->emplace(kDBusLogEntryLong, dbus::statistics::GetAsString(
                                           dbus::statistics::SHOW_METHOD,
                                           dbus::statistics::FORMAT_TOTALS));
  std::move(callback).Run(std::move(response));
}

}  // namespace system_logs
