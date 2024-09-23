// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace enterprise_connectors {

class ExtensionTelemetryEventRouter;

class ExtensionTelemetryEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  ExtensionTelemetryEventRouterFactory(
      const ExtensionTelemetryEventRouterFactory&) = delete;
  ExtensionTelemetryEventRouterFactory& operator=(
      const ExtensionTelemetryEventRouterFactory&) = delete;

  // Returns the ExtensionTelemetryEventRouter for |context|, creating it if
  // it is not yet created.
  static ExtensionTelemetryEventRouter* GetForProfile(
      content::BrowserContext* context);

  // Returns the ExtensionTelemetryEventRouterFactory instance.
  static ExtensionTelemetryEventRouterFactory* GetInstance();

 protected:
  // BrowserContextKeyedServiceFactory overrides:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  friend base::NoDestructor<ExtensionTelemetryEventRouterFactory>;

  ExtensionTelemetryEventRouterFactory();
  ~ExtensionTelemetryEventRouterFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_TELEMETRY_EVENT_ROUTER_FACTORY_H_
