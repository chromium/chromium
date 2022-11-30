// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_CONTROLLER_H_

namespace autofill {

// An interface for interaction between the view and the corresponding UI
// controller on Android. Acts as the native counterpart for the Java
// TouchToFillCreditCardComponent.Delegate.
class TouchToFillCreditCardViewController {
 public:
  virtual ~TouchToFillCreditCardViewController() = default;

  virtual void OnDismissed(JNIEnv* env) = 0;
  virtual base::android::ScopedJavaLocalRef<jobject> GetJavaObject() = 0;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_VIEW_CONTROLLER_H_
