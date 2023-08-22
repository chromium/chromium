// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_FACTORY_H_
#define CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace extensions {

class WarningBadgeService;

class WarningBadgeServiceFactory : public ProfileKeyedServiceFactory {
 public:
  WarningBadgeServiceFactory(const WarningBadgeServiceFactory&) = delete;
  WarningBadgeServiceFactory& operator=(const WarningBadgeServiceFactory&) =
      delete;

  static WarningBadgeService* GetForBrowserContext(
      content::BrowserContext* context);
  static WarningBadgeServiceFactory* GetInstance();

 private:
  friend base::NoDestructor<WarningBadgeServiceFactory>;

  WarningBadgeServiceFactory();
  ~WarningBadgeServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_WARNING_BADGE_SERVICE_FACTORY_H_
