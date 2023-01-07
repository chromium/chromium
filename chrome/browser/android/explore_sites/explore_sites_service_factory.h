// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_
#define CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_

#include "chrome/browser/android/explore_sites/explore_sites_service.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}  // namespace base

namespace explore_sites {

class ExploreSitesService;

// A factory to create one ExploreSitesServiceImpl per browser context.
class ExploreSitesServiceFactory : public ProfileKeyedServiceFactory {
 public:
  static ExploreSitesServiceFactory* GetInstance();
  static ExploreSitesService* GetForBrowserContext(
      content::BrowserContext* context);

  ExploreSitesServiceFactory(const ExploreSitesServiceFactory&) = delete;
  ExploreSitesServiceFactory& operator=(const ExploreSitesServiceFactory&) =
      delete;

 private:
  friend struct base::DefaultSingletonTraits<ExploreSitesServiceFactory>;

  ExploreSitesServiceFactory();
  ~ExploreSitesServiceFactory() override;

  // BrowserContextKeyedServiceFactory implementation:
  bool ServiceIsCreatedWithBrowserContext() const override;

  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* context) const override;
};

}  // namespace explore_sites

#endif  // CHROME_BROWSER_ANDROID_EXPLORE_SITES_EXPLORE_SITES_SERVICE_FACTORY_H_
