// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;

namespace enterprise_reporting {

class CloudProfileReportingService;

class CloudProfileReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static CloudProfileReportingServiceFactory* GetInstance();

  static CloudProfileReportingService* GetForProfile(Profile* profile);

  CloudProfileReportingServiceFactory(
      const CloudProfileReportingServiceFactory&) = delete;
  CloudProfileReportingServiceFactory& operator=(
      const CloudProfileReportingServiceFactory&) = delete;

 protected:
  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<CloudProfileReportingServiceFactory>;

  CloudProfileReportingServiceFactory();
  ~CloudProfileReportingServiceFactory() override;
};

}  // namespace enterprise_reporting

#endif  // CHROME_BROWSER_ENTERPRISE_REPORTING_CLOUD_PROFILE_REPORTING_SERVICE_FACTORY_H_
