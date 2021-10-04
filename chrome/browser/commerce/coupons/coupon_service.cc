// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/commerce/coupons/coupon_service.h"

CouponService::CouponService(std::unique_ptr<CouponDB> coupon_db)
    : coupon_db_(std::move(coupon_db)) {}
CouponService::~CouponService() = default;
