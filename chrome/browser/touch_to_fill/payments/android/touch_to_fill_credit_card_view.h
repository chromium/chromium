// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_

namespace autofill {

class TouchToFillCreditCardViewController;

// The UI interface which prompts the user to select a credit card to fill
// using Touch To Fill surface.
class TouchToFillCreditCardView {
 public:
  virtual ~TouchToFillCreditCardView() = default;

  virtual bool Show(TouchToFillCreditCardViewController* controller) = 0;
  virtual void Hide() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_H_
