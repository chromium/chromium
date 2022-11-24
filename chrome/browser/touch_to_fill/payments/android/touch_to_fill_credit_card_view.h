// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_

#include "base/containers/span.h"

namespace autofill {

class TouchToFillCreditCardViewController;
class CreditCard;

// The UI interface which prompts the user to select a credit card to fill
// using Touch To Fill surface.
class TouchToFillCreditCardView {
 public:
  virtual ~TouchToFillCreditCardView() = default;

  virtual bool Show(
      TouchToFillCreditCardViewController* controller,
      base::span<const autofill::CreditCard* const> cards_to_suggest,
      bool should_show_scan_credit_card) = 0;
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_
