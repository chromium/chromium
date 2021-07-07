// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_
#define CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_

// This is used to collect metric related to the Cart Discount.
class CartDiscountMetricCollector {
 public:
  // Gets called when Chrome fetches for discount. It increments the number of
  // discount fetches.
  static void RecordFetchingForDiscounts();
  // Gets called when the user clicks on the discounted cart. It increments
  // the number of discounted url fetches.
  static void RecordFetchingForDiscountedLink();
  // Gets called when chrome receives the discounted cart url. It increments
  // the number of discounted link is used.
  static void RecordAppliedDiscount();
  // Gets called when the user clicks on the cart item. It records whether
  // the cart has discounts.
  static void RecordClickedOnDiscount(bool has_discounts);
};

#endif  // CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_
