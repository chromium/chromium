// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_
#define CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_

#include "components/commerce/core/commerce_feature_list.h"

// This is used to collect metric related to the Cart Discount.
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
class CartDiscountMetricCollector {
 public:
  // Represent the status of discount consent.
  enum class DiscountConsentStatus {
    // User has seen and accepted the consent.
    ACCEPTED = 0,
    // User has seen and decilined the consent.
    DECLINED = 1,
    // User has seen the consent before but never acted on it, and the consent
    // is showing now.
    IGNORED = 2,
    // User has seen the consent before but never acted on it, and the consent
    // is not showing now.
    NO_SHOW = 3,
    // User has never seen the consent.
    NEVER_SHOWN = 4,
    kMaxValue = NEVER_SHOWN,
  };

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
  // Gets called when cart module shows to record histogram for discount consent
  // status.
  static void RecordDiscountConsentStatus(DiscountConsentStatus status);
  // The following get called when cart module shows to record histogram for
  // detail discount consent status.
  static void RecordDiscountConsentStatusAcceptedIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusRejectedIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusNoShowAfterDecidedIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusDismissedIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusShowInterestIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusNeverShowIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusNoShowIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusIgnoredIn(
      commerce::DiscountConsentNtpVariation variation);
  static void RecordDiscountConsentStatusShownIn(
      commerce::DiscountConsentNtpVariation variation);
};

#endif  // CHROME_BROWSER_CART_CART_DISCOUNT_METRIC_COLLECTOR_H_
