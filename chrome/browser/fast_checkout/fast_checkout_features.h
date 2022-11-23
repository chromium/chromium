// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_FEATURES_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_FEATURES_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"
#include "build/build_config.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)
// Feature that enables Fast Checkout experiences on Android to help users
// speed up the checkout process.
BASE_DECLARE_FEATURE(kFastCheckout);

// Force enables fast checkout capabilities for every domain, regardless of
// the server response. The flag is meant for end-to-end testing purposes only.
BASE_DECLARE_FEATURE(kForceEnableFastCheckoutCapabilities);
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_FEATURES_H_
