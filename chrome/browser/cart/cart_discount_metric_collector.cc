// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/cart/cart_discount_metric_collector.h"

#include "base/hash/hash.h"
#include "base/metrics/histogram_functions.h"
#include "components/search/ntp_features.h"

namespace {
// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
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

void CartDiscountMetricCollector::RecordDiscountConsentStatus(
    DiscountConsentStatus status) {
  base::UmaHistogramEnumeration("NewTabPage.Carts.DiscountConsentStatusAtLoad",
                                status);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusAcceptedIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.AcceptedIn", variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusRejectedIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.RejectedIn", variation);
}

void CartDiscountMetricCollector::
    RecordDiscountConsentStatusNoShowAfterDecidedIn(
        commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowHasFinalized",
      variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusDismissedIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.DismissedIn", variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusShowInterestIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.InterestedButNoActionIn",
      variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusNeverShowIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NeverShownIn", variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusNoShowIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.NoShowIn", variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusIgnoredIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.IgnoredIn", variation);
}

void CartDiscountMetricCollector::RecordDiscountConsentStatusShownIn(
    commerce::DiscountConsentNtpVariation variation) {
  base::UmaHistogramEnumeration(
      "NewTabPage.Carts.DiscountConsentStatusAtLoad.ShownIn", variation);
}
