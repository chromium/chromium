// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_

#include <memory>

#include "components/enterprise/browser/reporting/reporting_delegate_factory.h"

class Profile;

namespace enterprise_reporting {

// Android implementation of the reporting delegate factory. Creates android-
// specific delegates for the enterprise reporting classes.
class ReportingDelegateFactoryAndroid : public ReportingDelegateFactory {
 public:
  ReportingDelegateFactoryAndroid() = default;
  ReportingDelegateFactoryAndroid(const ReportingDelegateFactoryAndroid&) =
      delete;
  ReportingDelegateFactoryAndroid& operator=(
      const ReportingDelegateFactoryAndroid&) = delete;
  ~ReportingDelegateFactoryAndroid() override = default;

  // ReportingDelegateFactory implementation.
  std::unique_ptr<BrowserReportGenerator::Delegate>
  GetBrowserReportGeneratorDelegate() const override;
  std::unique_ptr<ProfileReportGenerator::Delegate>
  GetProfileReportGeneratorDelegate() const override;
  std::unique_ptr<ReportGenerator::Delegate> GetReportGeneratorDelegate()
      const override;
  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate()
      const override;
  std::unique_ptr<RealTimeReportGenerator::Delegate>
  GetRealTimeReportGeneratorDelegate() const override;
  std::unique_ptr<RealTimeReportController::Delegate>
  GetRealTimeReportControllerDelegate() const override;

  std::unique_ptr<ReportScheduler::Delegate> GetReportSchedulerDelegate(
      Profile* profile) const;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_REPORTING_DELEGATE_FACTORY_ANDROID_H_
