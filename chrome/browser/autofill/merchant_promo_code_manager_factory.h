// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_

#include "chrome/browser/profiles/profile_keyed_service_factory.h"

namespace base {
template <typename T>
class NoDestructor;
}

class Profile;

namespace autofill {

class MerchantPromoCodeManager;

// Singleton that owns all MerchantPromoCodeManagers and associates
// them with Profiles. Listens for the Profile's destruction notification and
// cleans up the associated MerchantPromoCodeManager.
class MerchantPromoCodeManagerFactory : public ProfileKeyedServiceFactory {
 public:
  // Returns the MerchantPromoCodeManager for |profile|, creating it
  // if it is not yet created.
  static MerchantPromoCodeManager* GetForProfile(Profile* profile);

  static MerchantPromoCodeManagerFactory* GetInstance();

 private:
  friend base::NoDestructor<MerchantPromoCodeManagerFactory>;

  MerchantPromoCodeManagerFactory();
  ~MerchantPromoCodeManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_
