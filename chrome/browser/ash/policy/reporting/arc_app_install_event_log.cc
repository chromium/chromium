// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_log.h"

#include "components/policy/proto/device_management_backend.pb.h"

namespace em = enterprise_management;

namespace policy {

ArcAppInstallEventLog::ArcAppInstallEventLog(const base::FilePath& file_name)
    : InstallEventLog(file_name) {}

ArcAppInstallEventLog::~ArcAppInstallEventLog() = default;

void ArcAppInstallEventLog::Serialize(em::AppInstallReportRequest* report) {
  report->Clear();
  for (const auto& log : logs_) {
    em::AppInstallReport* const report_log = report->add_app_install_reports();
    log.second->Serialize(report_log);
  }
}

}  // namespace policy
