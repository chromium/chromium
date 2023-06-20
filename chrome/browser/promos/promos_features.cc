// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/promos/promos_features.h"

namespace promos_features {
// This feature controls whether the user can be shown the Chrome for iOS promo
// when saving/updating their passwords.
BASE_FEATURE(kIOSPromoPasswordBubble,
             "IOSPromoPasswordBubble",
             base::FEATURE_DISABLED_BY_DEFAULT);

// This array lists the different activation params that can be passed in the
// experiment config, with their corresponding string.
constexpr base::FeatureParam<IOSPromoPasswordBubbleActivation>::Option
    kIOSPromoPasswordBubbleActivationOptions[] = {
        {IOSPromoPasswordBubbleActivation::kContextualDirect,
         "contextual-direct"},
        {IOSPromoPasswordBubbleActivation::kContextualIndirect,
         "contextual-indirect"},
        {IOSPromoPasswordBubbleActivation::kNonContextualDirect,
         "non-contextual-direct"},
        {IOSPromoPasswordBubbleActivation::kNonContextualIndirect,
         "non-contextual-indirect"},
        {IOSPromoPasswordBubbleActivation::kAlwaysShowWithPasswordBubbleDirect,
         "always-show-direct"},
        {IOSPromoPasswordBubbleActivation::
             kAlwaysShowWithPasswordBubbleIndirect,
         "always-show-indirect"}};

constexpr base::FeatureParam<IOSPromoPasswordBubbleActivation>
    kIOSPromoPasswordBubbleActivationParam{
        &kIOSPromoPasswordBubble, "activation",
        IOSPromoPasswordBubbleActivation::kContextualDirect,
        &kIOSPromoPasswordBubbleActivationOptions};
}  // namespace promos_features
