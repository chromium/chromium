// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_SYSTEM_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_SYSTEM_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/extensions/chrome_extension_system.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/browser/extension_system_provider.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {
class ExtensionSystem;

// BrowserContextKeyedServiceFactory for ChromeExtensionSystem::Shared.
// Should not be used except by ExtensionSystem(Factory).
class ChromeExtensionSystemSharedFactory : public ProfileKeyedServiceFactory {
 public:
  ChromeExtensionSystemSharedFactory(
      const ChromeExtensionSystemSharedFactory&) = delete;
  ChromeExtensionSystemSharedFactory& operator=(
      const ChromeExtensionSystemSharedFactory&) = delete;

  static ChromeExtensionSystem::Shared* GetForBrowserContext(
      content::BrowserContext* context);

  static ChromeExtensionSystemSharedFactory* GetInstance();

 private:
  friend base::NoDestructor<ChromeExtensionSystemSharedFactory>;

  ChromeExtensionSystemSharedFactory();
  ~ChromeExtensionSystemSharedFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

// BrowserContextKeyedServiceFactory for ChromeExtensionSystem.
class ChromeExtensionSystemFactory : public ExtensionSystemProvider {
 public:
  ChromeExtensionSystemFactory(const ChromeExtensionSystemFactory&) = delete;
  ChromeExtensionSystemFactory& operator=(const ChromeExtensionSystemFactory&) =
      delete;

  // ExtensionSystem provider implementation:
  ExtensionSystem* GetForBrowserContext(
      content::BrowserContext* context) override;

  static ChromeExtensionSystemFactory* GetInstance();

 private:
  friend base::NoDestructor<ChromeExtensionSystemFactory>;

  ChromeExtensionSystemFactory();
  ~ChromeExtensionSystemFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_CHROME_EXTENSION_SYSTEM_FACTORY_H_
