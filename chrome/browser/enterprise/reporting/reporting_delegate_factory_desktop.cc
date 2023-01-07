// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/profile_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/real_time_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetBrowserReportGeneratorDelegate() {
  return std::make_unique<BrowserReportGeneratorDesktop>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetProfileReportGeneratorDelegate() {
  return std::make_unique<ProfileReportGeneratorDesktop>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetReportGeneratorDelegate() {
  return std::make_unique<ReportGeneratorDesktop>();
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryDesktop::GetReportSchedulerDelegate() {
  return std::make_unique<ReportSchedulerDesktop>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetRealTimeReportGeneratorDelegate() {
  return std::make_unique<RealTimeReportGeneratorDesktop>();
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryDesktop::GetReportSchedulerDelegate(Profile* profile) {
  return std::make_unique<ReportSchedulerDesktop>(profile,
                                                  /*profile_reporting=*/true);
}

}  // namespace enterprise_reporting
