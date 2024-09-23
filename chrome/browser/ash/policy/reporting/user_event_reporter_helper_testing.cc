// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/reporting/user_event_reporter_helper_testing.h"

namespace reporting {

UserEventReporterHelperTesting::~UserEventReporterHelperTesting() = default;

UserEventReporterHelperTesting::UserEventReporterHelperTesting(
    bool reporting_enabled,
    bool should_report_user,
    bool is_kiosk_user,
    std::unique_ptr<ReportQueue, base::OnTaskRunnerDeleter> report_queue)
    : UserEventReporterHelper(std::move(report_queue)),
      reporting_enabled_(reporting_enabled),
      should_report_user_(should_report_user),
      is_kiosk_user_(is_kiosk_user) {}

bool UserEventReporterHelperTesting::ReportingEnabled(
    const std::string&) const {
  return reporting_enabled_;
}

bool UserEventReporterHelperTesting::ShouldReportUser(
    const std::string&) const {
  return should_report_user_;
}

bool UserEventReporterHelperTesting::IsKioskUser() const {
  return is_kiosk_user_;
}

// static
std::string UserEventReporterHelperTesting::GetDeviceDmToken() const {
  return "testing-dm-token";
}

}  // namespace reporting
