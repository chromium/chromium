// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
#define CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_

#include "base/no_destructor.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

class Profile;

namespace performance_manager {

class SiteDataCacheFacade;

// BrowserContextKeyedServiceFactory that adorns each browser context with a
// SiteDataCacheFacade.
class SiteDataCacheFacadeFactory : public BrowserContextKeyedServiceFactory {
 public:
  static SiteDataCacheFacade* GetForProfile(Profile* profile);
  static SiteDataCacheFacadeFactory* GetInstance();

 private:
  friend class base::NoDestructor<SiteDataCacheFacadeFactory>;

  SiteDataCacheFacadeFactory();
  ~SiteDataCacheFacadeFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
  bool ServiceIsCreatedWithBrowserContext() const override;
  bool ServiceIsNULLWhileTesting() const override;

  DISALLOW_COPY_AND_ASSIGN(SiteDataCacheFacadeFactory);
};

}  // namespace performance_manager

#endif  // CHROME_BROWSER_PERFORMANCE_MANAGER_PERSISTENCE_SITE_DATA_SITE_DATA_CACHE_FACADE_FACTORY_H_
