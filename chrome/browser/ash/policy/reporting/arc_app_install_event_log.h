// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_H_

#include "chrome/browser/ash/policy/reporting/install_event_log.h"
#include "chrome/browser/ash/policy/reporting/single_arc_app_install_event_log.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class FilePath;
}  // namespace base

namespace policy {

// An event log for ARC++ app push-installs.
class ArcAppInstallEventLog
    : public InstallEventLog<enterprise_management::AppInstallReportLogEvent,
                             SingleArcAppInstallEventLog> {
 public:
  explicit ArcAppInstallEventLog(const base::FilePath& file_name);
  ~ArcAppInstallEventLog();

  // Serializes the log to a protobuf for upload to a server. Records which
  // entries were serialized so that they may be cleared after successful
  // upload.
  void Serialize(enterprise_management::AppInstallReportRequest* report);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_EVENT_LOG_H_
