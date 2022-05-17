// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_
#define CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_

#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

namespace base {
template <typename T>
struct DefaultSingletonTraits;
}

class Profile;

namespace autofill {

class MerchantPromoCodeManager;

// Singleton that owns all MerchantPromoCodeManagers and associates
// them with Profiles. Listens for the Profile's destruction notification and
// cleans up the associated MerchantPromoCodeManager.
class MerchantPromoCodeManagerFactory
    : public BrowserContextKeyedServiceFactory {
 public:
  // Returns the MerchantPromoCodeManager for |profile|, creating it
  // if it is not yet created.
  static MerchantPromoCodeManager* GetForProfile(Profile* profile);

  static MerchantPromoCodeManagerFactory* GetInstance();

 private:
  friend struct base::DefaultSingletonTraits<MerchantPromoCodeManagerFactory>;

  MerchantPromoCodeManagerFactory();
  ~MerchantPromoCodeManagerFactory() override;

  // BrowserContextKeyedServiceFactory:
  KeyedService* BuildServiceInstanceFor(
      content::BrowserContext* profile) const override;
  content::BrowserContext* GetBrowserContextToUse(
      content::BrowserContext* context) const override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_MERCHANT_PROMO_CODE_MANAGER_FACTORY_H_
