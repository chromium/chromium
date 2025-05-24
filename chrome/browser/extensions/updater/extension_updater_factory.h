// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace extensions {

class ExtensionUpdater;

// Factory for ExtensionUpdater objects. ExtensionUpdater objects are shared
// between an incognito browser context and its original browser context.
class ExtensionUpdaterFactory : public ProfileKeyedServiceFactory {
 public:
  ExtensionUpdaterFactory(const ExtensionUpdaterFactory&) = delete;
  ExtensionUpdaterFactory& operator=(const ExtensionUpdaterFactory&) = delete;

  static ExtensionUpdater* GetForBrowserContext(
      content::BrowserContext* context);

  static ExtensionUpdaterFactory* GetInstance();

 private:
  friend base::NoDestructor<ExtensionUpdaterFactory>;

  ExtensionUpdaterFactory();
  ~ExtensionUpdaterFactory() override;

  // ProfileKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_UPDATER_EXTENSION_UPDATER_FACTORY_H_
