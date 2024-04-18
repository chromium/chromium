// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_ENCRYPTED_EVENT_REPORTER_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_ENCRYPTED_EVENT_REPORTER_H_

#include "chrome/browser/ash/policy/reporting/arc_app_install_event_logger.h"
#include "components/reporting/client/report_queue.h"

namespace policy {

// Class that reports arc app install events using the encrypted reporting
// pipeline. Only the following events are reported (the rest are silently
// ignored):
// - AppInstallReportLogEvent::SUCCESS
// - AppInstallReportLogEvent::INSTALLATION_STARTED
// - AppInstallReportLogEvent::INSTALLATION_FAILED
// - AppInstallReportLogEvent::SERVER_REQUEST
class ArcAppInstallEncryptedEventReporter
    : public ArcAppInstallEventLogger::Delegate {
 public:
  explicit ArcAppInstallEncryptedEventReporter(
      std::unique_ptr<reporting::ReportQueue, base::OnTaskRunnerDeleter>
          report_queue,
      Profile* profile);

  ArcAppInstallEncryptedEventReporter(
      const ArcAppInstallEncryptedEventReporter&) = delete;

  ArcAppInstallEncryptedEventReporter operator=(
      const ArcAppInstallEncryptedEventReporter&) = delete;

  ~ArcAppInstallEncryptedEventReporter() override;

  // ArcAppInstallEventLogger::Delegate
  void Add(
      const std::set<std::string>& packages,
      const enterprise_management::AppInstallReportLogEvent& event) override;
  void GetAndroidId(
      ArcAppInstallEventLogger::Delegate::AndroidIdCallback) const override;

 private:
  std::unique_ptr<reporting::ReportQueue, base::OnTaskRunnerDeleter>
      report_queue_;

  // Collects log events and passes them to |this|.
  std::unique_ptr<ArcAppInstallEventLogger> logger_;
};

}  // namespace policy

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_ARC_APP_INSTALL_ENCRYPTED_EVENT_REPORTER_H_
