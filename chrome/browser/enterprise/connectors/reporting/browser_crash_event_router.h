// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

// Keyed service that manages adding and removing profiles from
// `CrashReportingContext`.
class BrowserCrashEventRouter : public KeyedService {
 public:
  explicit BrowserCrashEventRouter(content::BrowserContext* context);

  BrowserCrashEventRouter(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter& operator=(const BrowserCrashEventRouter&) = delete;
  BrowserCrashEventRouter(BrowserCrashEventRouter&&) = delete;
  BrowserCrashEventRouter& operator=(BrowserCrashEventRouter&&) = delete;
  ~BrowserCrashEventRouter() override;
};

class BrowserCrashEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static BrowserCrashEventRouterFactory* GetInstance();
  static BrowserCrashEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  BrowserCrashEventRouterFactory();
  ~BrowserCrashEventRouterFactory() override;
  friend struct base::DefaultSingletonTraits<BrowserCrashEventRouterFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_BROWSER_CRASH_EVENT_ROUTER_H_
