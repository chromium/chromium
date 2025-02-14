// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {

// The factory responsible for creating the event router for the permissions
// API.
class PermissionsEventRouterFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PermissionsEventRouterFactory instance.
  static PermissionsEventRouterFactory* GetInstance();

  PermissionsEventRouterFactory(const PermissionsEventRouterFactory&) = delete;
  PermissionsEventRouterFactory& operator=(
      const PermissionsEventRouterFactory&) = delete;

 private:
  friend base::NoDestructor<PermissionsEventRouterFactory>;

  PermissionsEventRouterFactory();
  ~PermissionsEventRouterFactory() override = default;

  // BrowserContextKeyedServiceFactory:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_PERMISSIONS_PERMISSIONS_EVENT_ROUTER_FACTORY_H_
