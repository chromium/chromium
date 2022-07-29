// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_

#include "chrome/browser/touch_to_fill/payments/touch_to_fill_credit_card_view.h"

namespace autofill {

class TouchToFillCreditCardViewImpl : public TouchToFillCreditCardView {
 public:
  TouchToFillCreditCardViewImpl();
  TouchToFillCreditCardViewImpl(const TouchToFillCreditCardViewImpl&) = delete;
  TouchToFillCreditCardViewImpl& operator=(
      const TouchToFillCreditCardViewImpl&) = delete;
  ~TouchToFillCreditCardViewImpl() override;

  // TouchToFillCreditCardView:
  bool Show() override;
  void Hide() override;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_
