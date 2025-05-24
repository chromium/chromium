// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_FACTORY_H_

#include "components/enterprise/connectors/core/reporting_event_router.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

class ReportingEventRouterFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ReportingEventRouterFactory* GetInstance();
  static ReportingEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  ReportingEventRouterFactory();
  ~ReportingEventRouterFactory() override;
  friend struct base::DefaultSingletonTraits<ReportingEventRouterFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_EVENT_ROUTER_FACTORY_H_
