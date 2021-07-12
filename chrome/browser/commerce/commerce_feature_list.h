// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_
#define CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_

#include "base/feature_list.h"
#include "base/metrics/field_trial_params.h"

namespace commerce {
extern const base::Feature kCommercePriceTracking;
extern const base::Feature kCommerceMerchantViewer;
extern const base::FeatureParam<bool> kDeleteAllMerchantsOnClearBrowsingHistory;
}  // namespace commerce

#endif  // CHROME_BROWSER_COMMERCE_COMMERCE_FEATURE_LIST_H_
