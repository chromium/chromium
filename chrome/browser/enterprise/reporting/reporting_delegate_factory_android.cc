// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/reporting/reporting_delegate_factory_android.h"

#include <memory>
#include <utility>

#include "chrome/browser/enterprise/reporting/browser_report_generator_android.h"
#include "chrome/browser/enterprise/reporting/profile_report_generator_android.h"
#include "chrome/browser/enterprise/reporting/real_time_report_controller_android.h"
#include "chrome/browser/enterprise/reporting/report_scheduler_android.h"

namespace enterprise_reporting {

std::unique_ptr<BrowserReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetBrowserReportGeneratorDelegate() const {
  return std::make_unique<BrowserReportGeneratorAndroid>();
}

std::unique_ptr<ProfileReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetProfileReportGeneratorDelegate() const {
  return std::make_unique<ProfileReportGeneratorAndroid>();
}

std::unique_ptr<ReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetReportGeneratorDelegate() const {
  return nullptr;
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryAndroid::GetReportSchedulerDelegate() const {
  return std::make_unique<ReportSchedulerAndroid>();
}

std::unique_ptr<RealTimeReportGenerator::Delegate>
ReportingDelegateFactoryAndroid::GetRealTimeReportGeneratorDelegate() const {
  // TODO(crbug.com/40189722) Implement RealTimeReportGenerator::Delegate for
  // Android
  return nullptr;
}

std::unique_ptr<RealTimeReportController::Delegate>
ReportingDelegateFactoryAndroid::GetRealTimeReportControllerDelegate() const {
  return std::make_unique<RealTimeReportControllerAndroid>();
}

std::unique_ptr<ReportScheduler::Delegate>
ReportingDelegateFactoryAndroid::GetReportSchedulerDelegate(
    Profile* profile) const {
  return std::make_unique<ReportSchedulerAndroid>(profile);
}

}  // namespace enterprise_reporting
