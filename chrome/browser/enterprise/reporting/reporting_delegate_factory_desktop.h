// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_DESKTOP_H_

#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

#include <memory>

#include "components/enterprise/browser/reporting/browser_report_generator.h"
#include "components/enterprise/browser/reporting/profile_report_generator.h"
#include "components/enterprise/browser/reporting/real_time_report_generator.h"
#include "components/enterprise/browser/reporting/report_generator.h"
#include "components/enterprise/browser/reporting/report_scheduler.h"

class Profile;

namespace enterprise_reporting {

// Desktop implementation of the reporting delegate factory. Creates desktop-
// specific delegates for the enterprise reporting classes.
class ReportingDelegateFactoryDesktop : public ReportingDelegateFactory {
 public:
  ReportingDelegateFactoryDesktop() = default;
  ReportingDelegateFactoryDesktop(const ReportingDelegateFactoryDesktop&) =
      delete;
  ReportingDelegateFactoryDesktop& operator=(
      const ReportingDelegateFactoryDesktop&) = delete;
  ~ReportingDelegateFactoryDesktop() override = default;

  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() override;

  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() override;

  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      override;

  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate(
      Profile* profile);
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
