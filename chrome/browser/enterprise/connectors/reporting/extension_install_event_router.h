// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_INSTALL_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_INSTALL_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

namespace extensions {
class ExtensionRegistry;
}

namespace enterprise_connectors {

// A keyed service that observes extension install events and uploads events to
// the chrome reporting server side API if the
// "OnSecurityEventEnterpriseConnector" policy is set to the proper value.
class ExtensionInstallEventRouter
    : public extensions::ExtensionRegistryObserver,
      public KeyedService {
 public:
  explicit ExtensionInstallEventRouter(content::BrowserContext* context);

  ExtensionInstallEventRouter(const ExtensionInstallEventRouter&) = delete;
  ExtensionInstallEventRouter& operator=(const ExtensionInstallEventRouter&) =
      delete;
  ExtensionInstallEventRouter(ExtensionInstallEventRouter&&) = delete;
  ExtensionInstallEventRouter& operator=(ExtensionInstallEventRouter&&) =
      delete;

  ~ExtensionInstallEventRouter() override;
  void OnExtensionInstalled(content::BrowserContext* browser_context,
                            const extensions::Extension* extension,
                            bool is_update) override;
  void OnExtensionUninstalled(content::BrowserContext* browser_context,
                              const extensions::Extension* extension,
                              extensions::UninstallReason reason) override;

 private:
  raw_ptr<RealtimeReportingClient, AcrossTasksDanglingUntriaged>
      reporting_client_ = nullptr;
  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged>
      extension_registry_ = nullptr;
  void ReportExtensionInstallEvent(const extensions::Extension* extension,
                                   const char* extension_action);
};

class ExtensionInstallEventRouterFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  static ExtensionInstallEventRouterFactory* GetInstance();
  static ExtensionInstallEventRouter* GetForBrowserContext(
      content::BrowserContext* context);

 protected:
  bool ServiceIsCreatedWithBrowserContext() const override;

 private:
  ExtensionInstallEventRouterFactory();
  ~ExtensionInstallEventRouterFactory() override;
  friend struct base::DefaultSingletonTraits<
      ExtensionInstallEventRouterFactory>;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_INSTALL_EVENT_ROUTER_H_
