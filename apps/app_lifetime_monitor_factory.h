// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APPS_APP_LIFETIME_MONITOR_FACTORY_H_
#define APPS_APP_LIFETIME_MONITOR_FACTORY_H_

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace apps {

class AppLifetimeMonitor;

// Singleton that owns all AppLifetimeMonitors and associates them with
// BrowserContexts. Listens for the BrowserContext's destruction notification
// and cleans up the associated AppLifetimeMonitor.
class AppLifetimeMonitorFactory : public BrowserContextKeyedServiceFactory {
 public:
  static AppLifetimeMonitor* GetForBrowserContext(
      content::BrowserContext* context);

  static AppLifetimeMonitorFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<AppLifetimeMonitorFactory>;

  AppLifetimeMonitorFactory();
  ~AppLifetimeMonitorFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace apps

#endif  // APPS_APP_LIFETIME_MONITOR_FACTORY_H_
