// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_FACTORY_H_
#define CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace content {
class BrowserContext;
}

class Profile;

namespace ash {

class SystemExtensionsProvider;

// Singleton that owns all SystemExtensionsFactories and associates them with
// Profiles.
class SystemExtensionsProviderFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // SystemExtensionsProvider is created automatically for appropriate profiles
  // e.g. the primary profile.
  static SystemExtensionsProvider* GetForProfileIfExists(Profile* profile);

  static SystemExtensionsProviderFactory& GetInstance();

 private:
  friend base::NoDestructor<SystemExtensionsProviderFactory>;

  SystemExtensionsProviderFactory();
  SystemExtensionsProviderFactory(const SystemExtensionsProviderFactory&) =
      delete;
  SystemExtensionsProviderFactory& operator=(
      const SystemExtensionsProviderFactory&) = delete;
  ~SystemExtensionsProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_FACTORY_H_
