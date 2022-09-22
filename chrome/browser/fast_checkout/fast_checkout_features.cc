// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_features.h"

namespace features {

#if BUILDFLAG(IS_ANDROID)

BASE_FEATURE(kFastCheckout, "FastCheckout", base::FEATURE_DISABLED_BY_DEFAULT);
BASE_FEATURE(kForceEnableFastCheckoutCapabilities,
             "ForceEnableFastCheckoutCapabilities",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace features
