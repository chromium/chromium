// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_features.h"

#include "base/no_destructor.h"
#include "chrome/browser/commerce/commerce_feature_list.h"
#include "third_party/re2/src/re2/re2.h"

namespace cart_features {

namespace {

constexpr base::FeatureParam<std::string> kPartnerMerchantPattern{
    &ntp_features::kNtpChromeCartModule, "partner-merchant-pattern",
    // This regex does not match anything.
    "\\b\\B"};

const re2::RE2& GetPartnerMerchantPattern() {
  re2::RE2::Options options;
  options.set_case_sensitive(false);
  static base::NoDestructor<re2::RE2> instance(kPartnerMerchantPattern.Get(),
                                               options);
  return *instance;
}

}  // namespace

bool IsRuleDiscountPartnerMerchant(const GURL& url) {
  const std::string& url_string = url.spec();
  return RE2::PartialMatch(
      re2::StringPiece(url_string.data(), url_string.size()),
      GetPartnerMerchantPattern());
}

bool IsPartnerMerchant(const GURL& url) {
  return commerce::IsCouponDiscountPartnerMerchant(url) ||
         IsRuleDiscountPartnerMerchant(url);
}

bool IsFakeDataEnabled() {
  return base::GetFieldTrialParamValueByFeature(
             ntp_features::kNtpChromeCartModule,
             ntp_features::kNtpChromeCartModuleDataParam) == "fake";
}

}  // namespace cart_features
