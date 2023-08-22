// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class SystemIndicatorManager;

// BrowserContextKeyedServiceFactory for each SystemIndicatorManager.
class SystemIndicatorManagerFactory : public ProfileKeyedServiceFactory {
 public:
  static SystemIndicatorManager* GetForContext(
      content::BrowserContext* context);

  static SystemIndicatorManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<SystemIndicatorManagerFactory>;

  SystemIndicatorManagerFactory();
  ~SystemIndicatorManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__
