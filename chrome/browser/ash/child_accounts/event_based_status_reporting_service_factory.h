// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace ash {
class EventBasedStatusReportingService;

// Singleton that owns all EventBasedStatusReportingService and associates
// them with BrowserContexts. Listens for the BrowserContext's destruction
// notification and cleans up the associated EventBasedStatusReportingService.
class EventBasedStatusReportingServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static EventBasedStatusReportingService* GetForBrowserContext(
      content::BrowserContext* context);

  static EventBasedStatusReportingServiceFactory* GetInstance();

  EventBasedStatusReportingServiceFactory(
      const EventBasedStatusReportingServiceFactory&) = delete;
  EventBasedStatusReportingServiceFactory& operator=(
      const EventBasedStatusReportingServiceFactory&) = delete;

 private:
  friend class base::NoDestructor<EventBasedStatusReportingServiceFactory>;

  EventBasedStatusReportingServiceFactory();
  ~EventBasedStatusReportingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_CHILD_ACCOUNTS_EVENT_BASED_STATUS_REPORTING_SERVICE_FACTORY_H_
