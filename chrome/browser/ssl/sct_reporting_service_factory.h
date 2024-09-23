// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class SCTReportingService;

class SCTReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SCTReportingServiceFactory(const SCTReportingServiceFactory&) = delete;
  const SCTReportingServiceFactory& operator=(
      const SCTReportingServiceFactory&) = delete;

  // Returns singleton instance of SCTReportingServiceFactory.
  static SCTReportingServiceFactory* GetInstance();

  // Returns the reporting service associated with |context|.
  // TODO(crbug.com/40706546): Determine if we need to explicitly handle
  // Incognito, or if relying on SBER is sufficient.
  static SCTReportingService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend base::NoDestructor<SCTReportingServiceFactory>;

  SCTReportingServiceFactory();
  ~SCTReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory overrides:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

#endif  // CHROME_BROWSER_SSL_SCT_REPORTING_SERVICE_FACTORY_H_
