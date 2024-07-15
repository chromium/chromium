// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service_factory.h"

#include "base/no_destructor.h"
#include "chrome/browser/commerce/coupons/coupon_db.h"
#include "chrome/browser/commerce/coupons/coupon_service.h"
#include "content/public/browser/storage_partition.h"

// static
CouponServiceFactory* CouponServiceFactory::GetInstance() {
  static base::NoDestructor<CouponServiceFactory> factory;
  return factory.get();
}

// static
CouponService* CouponServiceFactory::GetForProfile(Profile* profile) {
  return static_cast<CouponService*>(
      GetInstance()->GetServiceForBrowserContext(profile, true));
}

CouponServiceFactory::CouponServiceFactory()
    : ProfileKeyedServiceFactory(
          "CouponService",
          ProfileSelections::Builder()
              .WithRegular(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/40257657): Check if this service is needed in
              // Guest mode.
              .WithGuest(ProfileSelection::kOriginalOnly)
              // TODO(crbug.com/41488885): Check if this service is needed for
              // Ash Internals.
              .WithAshInternals(ProfileSelection::kOriginalOnly)
              .Build()) {}

CouponServiceFactory::~CouponServiceFactory() = default;

std::unique_ptr<KeyedService>
CouponServiceFactory::BuildServiceInstanceForBrowserContext(
    content::BrowserContext* context) const {
  DCHECK(!context->IsOffTheRecord());

  return std::make_unique<CouponService>(std::make_unique<CouponDB>(context));
}
