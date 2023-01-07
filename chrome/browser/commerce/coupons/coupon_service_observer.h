// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_OBSERVER_H_
#define CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_OBSERVER_H_

#include "components/autofill/core/browser/data_model/autofill_offer_data.h"

// Interface class used to get notifications from CouponService.
class CouponServiceObserver : public base::CheckedObserver {
 public:
  // Gets called when |offer_data| is no longer valid.
  virtual void OnCouponInvalidated(
      const autofill::AutofillOfferData& offer_data) = 0;
};

#endif  // CHROME_BROWSER_COMMERCE_COUPONS_COUPON_SERVICE_OBSERVER_H_
