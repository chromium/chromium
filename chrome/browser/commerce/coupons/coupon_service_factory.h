// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_FACTORY_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_FACTORY_H_

#include "base/no_destructor.h"
#include "chrome/browser/profiles/profile_keyed_service_factory.h"

class Profile;
class CouponService;

// Factory to create CouponService per profile. CouponService is not supported
// on incognito, and the factory will return nullptr for an incognito profile.
class CouponServiceFactory : public ProfileKeyedServiceFactory {
 public:
  // Acquire instance of CouponServiceFactory.
  static CouponServiceFactory* GetInstance();

  // Acquire CouponService - there is one per profile.
  static CouponService* GetForProfile(Profile* profile);

 private:
  friend base::NoDestructor<CouponServiceFactory>;

  CouponServiceFactory();
  ~CouponServiceFactory() override;

  std::unique_ptr<KeyedService> BuildServiceInstanceForBrowserContext(
      content::BrowserContext* context) const override;
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_FACTORY_H_
