// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view.h"

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"

namespace content {
class WebContents;
}

namespace autofill {

class TouchToFillCreditCardViewController;
class CreditCard;

// Android implementation of the surface to select a credit card to fill.
// Uses Java TouchToFillCreditCardComponent to present a bottom sheet.
class TouchToFillCreditCardViewImpl : public TouchToFillCreditCardView {
 public:
  explicit TouchToFillCreditCardViewImpl(content::WebContents* web_contents);
  TouchToFillCreditCardViewImpl(const TouchToFillCreditCardViewImpl&) = delete;
  TouchToFillCreditCardViewImpl& operator=(
      const TouchToFillCreditCardViewImpl&) = delete;
  ~TouchToFillCreditCardViewImpl() override;

 private:
  // TouchToFillCreditCardView:
  bool Show(TouchToFillCreditCardViewController* controller,
            base::span<const autofill::CreditCard> cards_to_suggest,
            bool should_show_scan_credit_card) override;
  void Hide() override;

  // The corresponding Java TouchToFillCreditCardViewBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  raw_ptr<content::WebContents> web_contents_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_IMPL_H_
