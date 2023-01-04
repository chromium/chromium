// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_INSTALL_EVENT_ROUTER_H_
#define CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_EXTENSION_INSTALL_EVENT_ROUTER_H_

#include "base/memory/raw_ptr.h"
#include "chrome/browser/enterprise/connectors/reporting/realtime_reporting_client.h"
#include "extensions/browser/extension_registry_observer.h"

namespace extensions {
class ExtensionRegistry;
}

namespace enterprise_connectors {

// An event router that observes Safe Browsing events and notifies listeners.
// The router also uploads events to the chrome reporting server side API if
// the kRealtimeReportingFeature feature is enabled.
class ExtensionInstallEventRouter
    : public extensions::ExtensionRegistryObserver {
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

  void StartObserving();

 private:
  raw_ptr<enterprise_connectors::RealtimeReportingClient, DanglingUntriaged>
      reporting_client_ = nullptr;
  raw_ptr<extensions::ExtensionRegistry, DanglingUntriaged>
      extension_registry_ = nullptr;
};

}  // namespace enterprise_connectors

#endif  // CHROME_BROWSER_ENTERPRISE_CONNECTORS_REPORTING_REPORTING_SERVICE_SETTINGS_H_
