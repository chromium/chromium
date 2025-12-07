// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_TESTING_H_
#define CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_TESTING_H_

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper.h"

namespace reporting {

class UserEventReporterHelperTesting : public UserEventReporterHelper {
 public:
  UserEventReporterHelperTesting(
      bool reporting_enabled,
      bool should_report_user,
      bool is_kiosk_user,
      std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue);
  ~UserEventReporterHelperTesting() override;

  bool ReportingEnabled(const std::string&) const override;

  bool ShouldReportUser(const std::string&) const override;

  bool IsKioskUser() const override;

  std::string GetDeviceDmToken() const override;

 private:
  const bool reporting_enabled_;
  const bool should_report_user_;
  const bool is_kiosk_user_;
};
}  // namespace reporting

#endif  // CHROME_BROWSER_ASH_POLICY_REPORTING_USER_EVENT_REPORTER_HELPER_TESTING_H_
