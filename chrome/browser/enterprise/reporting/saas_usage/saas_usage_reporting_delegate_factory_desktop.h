// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_delegate_factory.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

class SaasUsageReportingDelegateFactoryDesktop
    : public SaasUsageReportingDelegateFactory {
 public:
  static std::unique_ptr<SaasUsageReportingDelegateFactoryDesktop>
  CreateForBrowser();
  static std::unique_ptr<SaasUsageReportingDelegateFactoryDesktop>
  CreateForProfile(Profile* profile);

  SaasUsageReportingDelegateFactoryDesktop(
      const SaasUsageReportingDelegateFactoryDesktop&) = delete;
  SaasUsageReportingDelegateFactoryDesktop& operator=(
      const SaasUsageReportingDelegateFactoryDesktop&) = delete;
  ~SaasUsageReportingDelegateFactoryDesktop() override = default;

  // SaasUsageReportingDelegateFactory implementation.
  PrefService* GetPrefService() const override;

  std::unique_ptr<SaasUsageReportFactory::Delegate>
  GetSaasUsageReportFactoryDelegate() const override;

  std::unique_ptr<SaasUsageReportUploader> GetSaasUsageReportUploader()
      const override;

  std::unique_ptr<SaasUsageReportScheduler::Delegate>
  GetSaasUsageReportSchedulerDelegate() const override;

 private:
  explicit SaasUsageReportingDelegateFactoryDesktop(Profile* profile);

  // `profile_` is null for browser-level reporting.
  raw_ptr<Profile> profile_ = nullptr;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
