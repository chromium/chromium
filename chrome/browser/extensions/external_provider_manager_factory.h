// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ExternalProviderManager;

// Factory for ExternalProviderManager objects. ExternalProviderManager objects
// are shared between an incognito browser context and its original browser
// context.
class ExternalProviderManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ExternalProviderManagerFactory(const ExternalProviderManagerFactory&) =
      delete;
  ExternalProviderManagerFactory& operator=(
      const ExternalProviderManagerFactory&) = delete;

  static ExternalProviderManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ExternalProviderManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<ExternalProviderManagerFactory>;

  ExternalProviderManagerFactory();
  ~ExternalProviderManagerFactory() override;

  // ProfileKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PROVIDER_MANAGER_FACTORY_H_
