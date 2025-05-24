// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_

#include "base/containers/span.h"

namespace autofill {

class Iban;
class LoyaltyCard;
struct Suggestion;
class TouchToFillPaymentMethodViewController;

// The UI interface which prompts the user to select a credit card to fill
// using Touch To Fill surface.
class TouchToFillPaymentMethodView {
 public:
  virtual ~TouchToFillPaymentMethodView() = default;

  virtual bool ShowCreditCards(
      TouchToFillPaymentMethodViewController* controller,
      base::span<const Suggestion> suggestions,
      bool should_show_scan_credit_card) = 0;
  virtual bool ShowIbans(TouchToFillPaymentMethodViewController* controller,
                         base::span<const Iban> ibans_to_suggest) = 0;
  virtual bool ShowLoyaltyCards(
      TouchToFillPaymentMethodViewController* controller,
      base::span<const LoyaltyCard> loyalty_cards_to_suggest) = 0;
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_VIEW_H_
