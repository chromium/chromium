// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROMOS_PROMOS_FEATURES_H_
#define CHROME_BROWSER_PROMOS_PROMOS_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace promos_features {
BASE_DECLARE_FEATURE(kIOSPromoPasswordBubble);

// This enum lists the possible params for the kIOSPromoPasswordBubble promo.
// The first two are the launch candidates, the second two are for experimental
// groups and the last two are for debugging/testing. Direct/indirect refers to
// the experiment variant (whether showing the user a QR directly or show them a
// landing page first).
enum class IOSPromoPasswordBubbleActivation {
  kContextualDirect,
  kContextualIndirect,
  kNonContextualDirect,
  kNonContextualIndirect,
  kAlwaysShowWithPasswordBubbleDirect,
  kAlwaysShowWithPasswordBubbleIndirect,
};
extern const base::FeatureParam<IOSPromoPasswordBubbleActivation>
    kIOSPromoPasswordBubbleActivationParam;

}  // namespace promos_features

#endif  // CHROME_BROWSER_PROMOS_PROMOS_FEATURES_H_
