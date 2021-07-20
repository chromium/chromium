// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/extension_install_event_log.h"

namespace em = enterprise_management;

namespace policy {

ExtensionInstallEventLog::ExtensionInstallEventLog(
    const base::FilePath& file_name)
    : InstallEventLog(file_name) {}

ExtensionInstallEventLog::~ExtensionInstallEventLog() = default;

void ExtensionInstallEventLog::Serialize(
    em::ExtensionInstallReportRequest* report) {
  report->Clear();
  for (const auto& log : logs_) {
    em::ExtensionInstallReport* const report_log =
        report->add_extension_install_reports();
    log.second->Serialize(report_log);
  }
}

}  // namespace policy
