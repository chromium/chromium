// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class ComponentLoader;

// Factory for ComponentLoader objects. ComponentLoader objects are shared
// between an incognito browser context and its original browser context.
class ComponentLoaderFactory : public ProfileKeyedServiceFactory {
 public:
  ComponentLoaderFactory(const ComponentLoaderFactory&) = delete;
  ComponentLoaderFactory& operator=(const ComponentLoaderFactory&) = delete;

  static ComponentLoader* GetForBrowserContext(
      content::BrowserContext* context);

  static ComponentLoaderFactory* GetInstance();

 private:
  friend base::NoDestructor<ComponentLoaderFactory>;

  ComponentLoaderFactory();
  ~ComponentLoaderFactory() override;

  // ProfileKeyedServiceFactory implementation:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_COMPONENT_LOADER_FACTORY_H_
