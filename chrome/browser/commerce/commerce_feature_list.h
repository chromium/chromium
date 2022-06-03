// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_
#define CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "url/gurl.h"

namespace commerce {
extern const base::Feature kCommercePriceTracking;
extern const base::Feature kCommerceMerchantViewer;
extern const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory;
extern const base::Feature kShoppingList;
extern const base::Feature kRetailCoupons;
extern const base::Feature kCommerceDeveloper;
// Parameter for enabling feature variation of coupons with code.
extern const char kRetailCouponsWithCodeParam[];

// Interval that controls the frequency of showing coupons in infobar bubbles.
constexpr base::FeatureParam<base::TimeDelta> kCouponDisplayInterval{
    &commerce::kRetailCoupons, "coupon_display_interval", base::Hours(18)};

// Check if a URL belongs to a partner merchant of coupon discount.
bool IsCouponDiscountPartnerMerchant(const GURL& url);
// Check if the feature variation of coupons with code is enabled.
bool IsCouponWithCodeEnabled();
}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_
