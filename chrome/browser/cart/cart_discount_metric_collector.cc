// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_metric_collector.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"

namespace {
enum CartDataRequestType {
  kCartDiscountInfo = 0,
  kCartDiscountUrl = 1,
  kMaxValue = kCartDiscountUrl,
};
}  // namespace

void CartDiscountMetricCollector::RecordFetchingForDiscounts() {
  base::UmaHistogramSparse("NewTabPage.Carts.DataRequest",
                           CartDataRequestType::kCartDiscountInfo);
  base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                           base::PersistentHash("chrome_cart"));
}

void CartDiscountMetricCollector::RecordFetchingForDiscountedLink() {
  base::UmaHistogramSparse("NewTabPage.Carts.DataRequest",
                           CartDataRequestType::kCartDiscountUrl);
  base::UmaHistogramSparse("NewTabPage.Modules.DataRequest",
                           base::PersistentHash("chrome_cart"));
}

void CartDiscountMetricCollector::RecordAppliedDiscount() {
  base::UmaHistogramSparse("NewTabPage.Carts.AppliedDiscount", 1);
}

void CartDiscountMetricCollector::RecordClickedOnDiscount(bool has_discounts) {
  base::UmaHistogramBoolean("NewTabPage.Carts.ClickCart.HasDiscount",
                            has_discounts);
}