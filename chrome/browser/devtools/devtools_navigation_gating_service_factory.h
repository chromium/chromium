// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_FACTORY_H_
#define CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class DevToolsNavigationGatingService;

class DevToolsNavigationGatingServiceFactory
    : public ProfileKeyedServiceFactory {
 public:
  static DevToolsNavigationGatingServiceFactory* GetInstance();
  static DevToolsNavigationGatingService* GetForBrowserContext(
      content::BrowserContext* context);

  DevToolsNavigationGatingServiceFactory(
      const DevToolsNavigationGatingServiceFactory&) = delete;
  DevToolsNavigationGatingServiceFactory& operator=(
      const DevToolsNavigationGatingServiceFactory&) = delete;

 private:
  friend base::NoDestructor<DevToolsNavigationGatingServiceFactory>;

  DevToolsNavigationGatingServiceFactory();
  ~DevToolsNavigationGatingServiceFactory() override;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVTOOLS_NAVIGATION_GATING_SERVICE_FACTORY_H_
