// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_

#include <memory>

#include "base/memory/raw_ptr.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_scheduler.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_uploader.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_delegate_factory.h"
#include "components/prefs/pref_service.h"

namespace enterprise_reporting {

class SaasUsageReportingDelegateFactoryDesktop
    : public SaasUsageReportingDelegateFactory {
 public:
  SaasUsageReportingDelegateFactoryDesktop() = default;
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
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_DELEGATE_FACTORY_DESKTOP_H_
