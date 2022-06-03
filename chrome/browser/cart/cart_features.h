// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_FEATURES_H_
#define CHROME_BROWSER_CART_CART_FEATURES_H_

#include "base/feature_list.h"
#include "components/search/ntp_features.h"
#include "url/gurl.h"

namespace cart_features {
// Default value is 6 hours.
constexpr base::FeatureParam<base::TimeDelta> kDiscountFetchDelayParam(
    &ntp_features::kNtpChromeCartModule,
    "discount-fetch-delay",
    base::Hours(6));

// Check if a URL belongs to a partner merchant of rule discount.
bool IsRuleDiscountPartnerMerchant(const GURL& url);

// Check if a URL belongs to a partner merchant of any discount types.
// TODO(crbug.com/1253633): Move this method to commerce_feature_list after
// modularizing CartService-related components.
bool IsPartnerMerchant(const GURL& url);

// Check if the variation with fake data is enabled.
bool IsFakeDataEnabled();
}  // namespace cart_features

#endif  // CHROME_BROWSER_CART_CART_FEATURES_H_
