// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_EXTENSION_INSTALL_EVENT_LOG_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_EXTENSION_INSTALL_EVENT_LOG_H_

#include <memory>
#include <string>

#include "chrome/browser/ash/policy/reporting/single_install_event_log.h"
#include "components/policy/proto/device_management_backend.pb.h"

namespace base {
class File;
}

namespace policy {

// An event log for a single extension's install process.
class SingleExtensionInstallEventLog
    : public SingleInstallEventLog<
          enterprise_management::ExtensionInstallReportLogEvent> {
 public:
  explicit SingleExtensionInstallEventLog(const std::string& extension_id);
  ~SingleExtensionInstallEventLog();

  // Restores the event log from |file| into |log|. If not even the extension
  // name can be parsed, |log| is set to nullptr and false returned.
  // |InstallEventLog::incomplete_| is set to |true| if it was set when storing
  // the log to the file. If the event log exceeds the size buffer,
  // |log| is created with |InstallEventLog::incomplete_| set to true, and false
  // is returned. Otherwise true is returned and if the buffer wraps around or
  // any log entries cannot be fully parsed, |InstallEventLog::incomplete_| is
  // set to true.
  static bool Load(base::File* file,
                   std::unique_ptr<SingleExtensionInstallEventLog>* log);

  // Serializes the log to a protobuf for upload to a server. Records which
  // entries were serialized so that they may be cleared after successful
  // upload.
  void Serialize(enterprise_management::ExtensionInstallReport* report);
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_SINGLE_EXTENSION_INSTALL_EVENT_LOG_H_
