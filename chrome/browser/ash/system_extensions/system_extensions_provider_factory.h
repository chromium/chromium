// Copyright 2021 The Chromium Authors
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

  SystemExtensionsProviderFactory(const SystemExtensionsProviderFactory&) =
      delete;
  SystemExtensionsProviderFactory& operator=(
      const SystemExtensionsProviderFactory&) = delete;

 private:
  friend base::NoDestructor<SystemExtensionsProviderFactory>;

  SystemExtensionsProviderFactory();
  ~SystemExtensionsProviderFactory() override;

  // BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  void RegisterProfilePrefs(
      user_prefs::PrefRegistrySyncable* registry) override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_SYSTEM_EXTENSIONS_SYSTEM_EXTENSIONS_PROVIDER_FACTORY_H_
