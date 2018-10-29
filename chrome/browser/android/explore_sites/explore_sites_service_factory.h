// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_

#include "base/macros.h"
#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace explore_sites {

class ExploreSitesService;

// A factory to create one ExploreSitesServiceImpl per browser context.
class ExploreSitesServiceFactory : public BrowserContextKeyedServiceFactory {
 public:
  static ExploreSitesServiceFactory* GetInstance();
  static ExploreSitesService* GetForBrowserContext(
      content::BrowserContext* context);

 private:
  friend struct base::DefaultSingletonTraits<ExploreSitesServiceFactory>;

  ExploreSitesServiceFactory();
  ~ExploreSitesServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;

  DISALLOW_COPY_AND_ASSIGN(ExploreSitesServiceFactory);
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_
