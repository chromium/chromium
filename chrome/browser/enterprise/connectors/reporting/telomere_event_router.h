// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_EVENT_ROUTER_H_

#include "base/feature_list.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "content/public/browser/browser_context.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace enterprise_connectors {

BASE_DECLARE_FEATURE(kTelomereReporting);

// Keyed service that manages adding and removing profiles from
// `TelomereReportingContext`.
class TelomereEventRouter : public KeyedService {
 public:
  explicit TelomereEventRouter(content::BrowserContext* context);

  TelomereEventRouter(const TelomereEventRouter&) = delete;
  TelomereEventRouter& operator=(const TelomereEventRouter&) = delete;
  TelomereEventRouter(TelomereEventRouter&&) = delete;
  TelomereEventRouter& operator=(TelomereEventRouter&&) = delete;
  ~TelomereEventRouter() override;
};

class TelomereEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  static TelomereEventRouterFactory* GetInstance();
  static TelomereEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

 private:
  TelomereEventRouterFactory();
  ~TelomereEventRouterFactory() override;
  friend struct base::DefaultSingletonTraits<TelomereEventRouterFactory>;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_TELOMERE_EVENT_ROUTER_H_
