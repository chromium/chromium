// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__

#include "base/memory/singleton.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

namespace extensions {
class SystemIndicatorManager;

// BrowserContextKeyedServiceFactory for each SystemIndicatorManager.
class SystemIndicatorManagerFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SystemIndicatorManager* GetForContext(
      content::BrowserContext* context);

  static SystemIndicatorManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<SystemIndicatorManagerFactory>;

  SystemIndicatorManagerFactory();
  ~SystemIndicatorManagerFactory() override;

  // BrowserContextKeyedServiceFactory implementation.
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_INDICATOR_SYSTEM_INDICATOR_MANAGER_FACTORY_H__
