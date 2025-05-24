// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_
#define CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_

#include "base/feature_list.h"
namespace page_info {

// Returns true if kPageInfoAboutThisSiteMoreInfo and dependent features are
// enabled.
bool IsAboutThisSiteFeatureEnabled();

// Enables the privacy policy insights Learning Experiment UI.
BASE_DECLARE_FEATURE(kPrivacyPolicyInsights);

bool IsMerchantTrustFeatureEnabled();

}  // namespace page_info

#endif  // CHROME_BROWSER_PAGE_INFO_PAGE_INFO_FEATURES_H_
