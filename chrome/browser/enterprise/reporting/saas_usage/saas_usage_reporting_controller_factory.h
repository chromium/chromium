// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/enterprise/browser/reporting/saas_usage/saas_usage_reporting_controller.h"

namespace enterprise_reporting {

class SaasUsageReportingControllerFactory : public ProfileKeyedServiceFactory {
 public:
  static SaasUsageReportingController* GetForProfile(Profile* profile);
  static SaasUsageReportingControllerFactory* GetInstance();

 private:
  friend class base::NoDestructor<SaasUsageReportingControllerFactory>;

  SaasUsageReportingControllerFactory();
  ~SaasUsageReportingControllerFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_SAAS_USAGE_SAAS_USAGE_REPORTING_CONTROLLER_FACTORY_H_
