// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_FEATURES_H_
#define CHROME_BROWSER_CART_CART_FEATURES_H_

#include "base/feature_list.h"
#include "components/search/ntp_features.h"

namespace cart_features {
constexpr base::FeatureParam<std::string> kPartnerMerchantPattern{
    &ntp_features::kNtpChromeCartModule, "partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

}  // namespace cart_features

#endif  // CHROME_BROWSER_CART_CART_FEATURES_H_
