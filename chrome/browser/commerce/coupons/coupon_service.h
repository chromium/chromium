// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/commerce/coupons/coupon_db.h"
#include "chrome/browser/commerce/coupons/coupon_service_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/keyed_service/core/keyed_service.h"

// Service to host coupon-related logics.
class CouponService : public KeyedService {
 public:
  CouponService(const CouponService&) = delete;
  CouponService& operator=(const CouponService&) = delete;
  ~CouponService() override;

 private:
  friend class CouponServiceFactory;

  // Use |CouponServiceFactory::GetForProfile(...)| to get an instance of this
  // service.
  explicit CouponService(std::unique_ptr<CouponDB> coupon_db);

  std::unique_ptr<CouponDB> coupon_db_;
  base::WeakPtrFactory<CouponService> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_H_
