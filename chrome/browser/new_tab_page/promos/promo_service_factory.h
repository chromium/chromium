// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_FACTORY_H_
#define CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class PromoService;
class Profile;

class PromoServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the PromoService for |profile|.
  static PromoService* GetForProfile(Profile* profile);

  static PromoServiceFactory* GetInstance();

  PromoServiceFactory(const PromoServiceFactory&) = delete;
  PromoServiceFactory& operator=(const PromoServiceFactory&) = delete;

 private:
  friend base::NoDestructor<PromoServiceFactory>;

  PromoServiceFactory();
  ~PromoServiceFactory() override;

  // Overridden from BrowserContextKeyedServiceFactory:
  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* profile) const override;
};

#endif  // CHROME_BROWSER_NEW_TAB_PAGE_PROMOS_PROMO_SERVICE_FACTORY_H_
