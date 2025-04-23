// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"
#include "extensions/buildflags/buildflags.h"

static_assert(BUILDFLAG(ENABLE_EXTENSIONS_CORE));

namespace content {
class BrowserContext;
}

namespace extensions {

class SharedModuleService;

// Factory for SharedModuleService objects. SharedModuleService objects are
// shared between an incognito browser context and its original browser context.
class SharedModuleServiceFactory : public ProfileKeyedServiceFactory {
 public:
  SharedModuleServiceFactory(const SharedModuleServiceFactory&) = delete;
  SharedModuleServiceFactory& operator=(const SharedModuleServiceFactory&) =
      delete;

  static SharedModuleService* GetForBrowserContext(
      content::BrowserContext* context);

  static SharedModuleServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<SharedModuleServiceFactory>;

  SharedModuleServiceFactory();
  ~SharedModuleServiceFactory() override;

  // ProfileKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_SHARED_MODULE_SERVICE_FACTORY_H_
