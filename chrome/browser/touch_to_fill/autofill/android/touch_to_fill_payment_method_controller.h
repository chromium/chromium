// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_view_controller.h"

namespace autofill {

class ContentAutofillClient;
class Iban;
class LoyaltyCard;
struct Suggestion;
class TouchToFillDelegate;
class TouchToFillPaymentMethodView;

// Controller of the bottom sheet surface for filling credit card, IBAN or
// loyalty card data on Android. It is responsible for showing the view and
// handling user interactions.
class TouchToFillPaymentMethodController
    : public TouchToFillPaymentMethodViewController {
 public:
  ~TouchToFillPaymentMethodController() override = default;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable credit
  // cards and be notified of the user's decision. `suggestions` are generated
  // using the `cards_to_suggest` data and include fields such as `main_text`,
  // `minor_text`, and `apply_deactivated_style`. The `apply_deactivated_style`
  // field determines which card suggestions should be disabled and grayed out
  // for the current merchant. Returns whether the surface was successfully
  // shown.
  virtual bool ShowCreditCards(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const Suggestion> suggestions) = 0;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable IBANs
  // and be notified of the user's decision. Returns whether the surface was
  // successfully shown.
  virtual bool ShowIbans(std::unique_ptr<TouchToFillPaymentMethodView> view,
                         base::WeakPtr<TouchToFillDelegate> delegate,
                         base::span<const Iban> ibans_to_suggest) = 0;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable
  // loyalty cards and be notified of the user's decision. `first_time_usage` is
  // true if the user has never seen the loyalty card IPH or the Touch To Fill
  // view before. Returns whether the surface was successfully shown.
  virtual bool ShowLoyaltyCards(
      std::unique_ptr<TouchToFillPaymentMethodView> view,
      base::WeakPtr<TouchToFillDelegate> delegate,
      base::span<const LoyaltyCard> affiliated_loyalty_cards,
      base::span<const LoyaltyCard> all_loyalty_cards,
      bool first_time_usage) = 0;

  // Hides the surface if it is currently shown.
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_AUTOFILL_ANDROID_TOUCH_TO_FILL_PAYMENT_METHOD_CONTROLLER_H_
