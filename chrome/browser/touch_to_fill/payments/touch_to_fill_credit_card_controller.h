// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_

#include <memory>

#include "base/memory/weak_ptr.h"

namespace autofill {

class TouchToFillCreditCardView;
class TouchToFillDelegate;

// Controller of the bottom sheet surface for filling credit card data. It is
// responsible for showing the view and handling user interactions.
class TouchToFillCreditCardController {
 public:
  TouchToFillCreditCardController();
  TouchToFillCreditCardController(const TouchToFillCreditCardController&) =
      delete;
  TouchToFillCreditCardController& operator=(
      const TouchToFillCreditCardController&) = delete;
  ~TouchToFillCreditCardController();

  // Shows the Touch To Fill |view|. |delegate| will provide the fillable credit
  // cards and be notified of the user's decision. Returns whether the surface
  // was successfully shown.
  bool Show(std::unique_ptr<TouchToFillCreditCardView> view,
            base::WeakPtr<TouchToFillDelegate> delegate);

  // Hides the surface if it is currently shown.
  void Hide();

 private:
  // Delegate for the surface being shown.
  base::WeakPtr<TouchToFillDelegate> delegate_;
  // View that displays the surface, owned by |this|.
  std::unique_ptr<TouchToFillCreditCardView> view_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_
