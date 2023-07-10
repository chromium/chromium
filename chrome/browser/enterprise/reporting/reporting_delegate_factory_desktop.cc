// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_desktop.h"

#include <memory>

#include "chrome/browser/enterprise/reporting/browser_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/profile_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/real_time_report_controller_desktop.h"
#include "chrome/browser/enterprise/reporting/real_time_report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/report_generator_desktop.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_desktop.h"
#include "components/enterprise/browser/reporting/real_time_report_controller.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetBrowserReportGeneratorDelegate() const {
  return std::make_unique<BrowserReportGeneratorDesktop>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetProfileReportGeneratorDelegate() const {
  return std::make_unique<ProfileReportGeneratorDesktop>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetReportGeneratorDelegate() const {
  return std::make_unique<ReportGeneratorDesktop>();
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryDesktop::GetReportSchedulerDelegate() const {
  return std::make_unique<ReportSchedulerDesktop>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryDesktop::GetRealTimeReportGeneratorDelegate() const {
  return std::make_unique<RealTimeReportGeneratorDesktop>();
}

std::unique_ptr<RealTimeReportController::Delegate>
ReportingDelegateFactoryDesktop::GetRealTimeReportControllerDelegate() const {
  return std::make_unique<RealTimeReportControllerDesktop>(profile_);
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryDesktop::GetReportSchedulerDelegate(
    Profile* profile) const {
  return std::make_unique<ReportSchedulerDesktop>(profile);
}

void ReportingDelegateFactoryDesktop::SetProfileForRealTimeController(
    Profile* profile) {
  profile_ = profile;
}

}  // namespace enterprise_reporting
