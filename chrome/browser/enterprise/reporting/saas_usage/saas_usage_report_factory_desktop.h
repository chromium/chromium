// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DESKTOP_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DESKTOP_H_

#include <optional>
#include <string>

#include "base/memory/raw_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_report_factory.h"

namespace enterprise_reporting {

// Desktop implementation of SaasUsageReportFactory::Delegate.
class SaasUsageReportFactoryDesktop final
    : public SaasUsageReportFactory::Delegate {
 public:
  explicit SaasUsageReportFactoryDesktop(Profile* profile);
  SaasUsageReportFactoryDesktop(const SaasUsageReportFactoryDesktop&) = delete;
  SaasUsageReportFactoryDesktop& operator=(
      const SaasUsageReportFactoryDesktop&) = delete;

  ~SaasUsageReportFactoryDesktop() override = default;

  // SaasUsageReportFactory::Delegate
  std::optional<std::string> GetProfileId() override;
  bool IsProfileAffiliated() override;

 private:
  // `profile_` is null for browser-level factory and non-null for
  // profile-level factory.
  raw_ptr<Profile> profile_;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORT_FACTORY_DESKTOP_H_
