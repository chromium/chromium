// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_ARC_APP_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_ARC_APP_INSTALL_EVENT_LOG_H_

#include <string>

#include "chrome/browser/ash/policy/reporting/single_install_event_log.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class File;
}

namespace policy {

// An event log for a single ARC++ app's push-install process.
class SingleArcAppInstallEventLog
    : public SingleInstallEventLog<
          enterprise_management::AppInstallReportLogEvent> {
 public:
  explicit SingleArcAppInstallEventLog(const std::string& package);
  ~SingleArcAppInstallEventLog();

  // Restores the event log from |file| into |log|. Returns |true| if the
  // self-delimiting format of the log was parsed successfully and further logs
  // stored in the file may be loaded.
  // |InstallEventLog::incomplete_| is set to |true| if it was set when storing
  // the log to the file, the buffer wraps around or any log entries cannot be
  // fully parsed. If not even the app name can be parsed, |log| is set to
  // |nullptr|.
  static bool Load(base::File* file,
                   std::unique_ptr<SingleArcAppInstallEventLog>* log);

  // Serializes the log to a protobuf for upload to a server. Records which
  // entries were serialized so that they may be cleared after successful
  // upload.
  void Serialize(enterprise_management::AppInstallReport* report);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_ARC_APP_INSTALL_EVENT_LOG_H_
