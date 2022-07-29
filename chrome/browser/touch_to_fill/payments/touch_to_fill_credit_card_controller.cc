// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/touch_to_fill_credit_card_controller.h"

#include "chrome/browser/touch_to_fill/payments/touch_to_fill_credit_card_view.h"
#include "components/autofill/core/browser/ui/touch_to_fill_delegate.h"

namespace autofill {

TouchToFillCreditCardController::TouchToFillCreditCardController() = default;
TouchToFillCreditCardController::~TouchToFillCreditCardController() = default;

bool TouchToFillCreditCardController::Show(
    std::unique_ptr<TouchToFillCreditCardView> view,
    base::WeakPtr<TouchToFillDelegate> delegate) {
  // Abort if TTF surface is already shown.
  if (view_)
    return false;

  if (!view->Show())
    return false;

  view_ = std::move(view);
  delegate_ = std::move(delegate);
  return true;
}

void TouchToFillCreditCardController::Hide() {
  if (view_) {
    view_->Hide();
    view_.reset();
    delegate_.reset();
  }
}

}  // namespace autofill
