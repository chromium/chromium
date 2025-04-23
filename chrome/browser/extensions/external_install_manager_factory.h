// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_FACTORY_H_

#include <memory>

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class ExternalInstallManager;

// Factory for ExternalInstallManager objects. ExternalInstallManager
// objects are shared between an incognito browser context and its original
// browser context.
class ExternalInstallManagerFactory : public ProfileKeyedServiceFactory {
 public:
  ExternalInstallManagerFactory(const ExternalInstallManagerFactory&) = delete;
  ExternalInstallManagerFactory& operator=(
      const ExternalInstallManagerFactory&) = delete;

  static ExternalInstallManager* GetForBrowserContext(
      content::BrowserContext* context);

  static ExternalInstallManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<ExternalInstallManagerFactory>;

  ExternalInstallManagerFactory();
  ~ExternalInstallManagerFactory() override;

  // ProfileKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_INSTALL_MANAGER_FACTORY_H_
