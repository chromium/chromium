// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_H_

#include "chrome/browser/ash/policy/reporting/install_event_log.h"
#include "chrome/browser/ash/policy/reporting/single_extension_install_event_log.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace policy {

// An event log for extension installs.
class ExtensionInstallEventLog
    : public InstallEventLog<
          enterprise_management::ExtensionInstallReportLogEvent,
          SingleExtensionInstallEventLog> {
 public:
  explicit ExtensionInstallEventLog(const base::FilePath& file_name);
  ~ExtensionInstallEventLog();

  // Serializes the log to a protobuf for upload to a server. Records which
  // entries were serialized so that they may be cleared after successful
  // upload.
  void Serialize(enterprise_management::ExtensionInstallReportRequest* report);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_EXTENSION_INSTALL_EVENT_LOG_H_
