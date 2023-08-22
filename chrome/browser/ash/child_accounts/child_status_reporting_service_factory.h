// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class ChildStatusReportingService;

// Singleton that owns all ChildStatusReportingService objects and associates
// them with corresponding BrowserContexts. Listens for the BrowserContext's
// destruction notification and cleans up the associated
// ChildStatusReportingService.
class ChildStatusReportingServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ChildStatusReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static ChildStatusReportingServiceFactory* GetInstance();

 private:
  friend class base::NoDestructor<ChildStatusReportingServiceFactory>;

  ChildStatusReportingServiceFactory();
  ChildStatusReportingServiceFactory(
      const ChildStatusReportingServiceFactory&) = delete;
  ChildStatusReportingServiceFactory& operator=(
      const ChildStatusReportingServiceFactory&) = delete;

  ~ChildStatusReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_CHILD_STATUS_REPORTING_SERVICE_FACTORY_H_
