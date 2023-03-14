// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_
#define CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_

#include <memory>

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/android/scoped_java_ref.h"
#include "base/containers/span.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_credit_card_view_controller.h"

namespace autofill {

class TouchToFillCreditCardView;
class TouchToFillDelegate;
class CreditCard;

// Controller of the bottom sheet surface for filling credit card data on
// Android. It is responsible for showing the view and handling user
// interactions. While the surface is shown, stores its Java counterpart in
// `java_object_`.
class TouchToFillCreditCardController
    : public TouchToFillCreditCardViewController {
 public:
  TouchToFillCreditCardController();
  TouchToFillCreditCardController(const TouchToFillCreditCardController&) =
      delete;
  TouchToFillCreditCardController& operator=(
      const TouchToFillCreditCardController&) = delete;
  ~TouchToFillCreditCardController() override;

  // Shows the Touch To Fill `view`. `delegate` will provide the fillable credit
  // cards and be notified of the user's decision. Returns whether the surface
  // was successfully shown.
  bool Show(std::unique_ptr<TouchToFillCreditCardView> view,
            base::WeakPtr<TouchToFillDelegate> delegate,
            base::span<const autofill::CreditCard> cards_to_suggest);

  // Hides the surface if it is currently shown.
  void Hide();

  // TouchToFillCreditCardViewController:
  void OnDismissed(JNIEnv* env, bool dismissed_by_user) override;
  void ScanCreditCard(JNIEnv* env) override;
  void ShowCreditCardSettings(JNIEnv* env) override;
  void SuggestionSelected(JNIEnv* env,
                          base::android::JavaParamRef<jstring> unique_id,
                          bool is_virtual) override;

 private:
  // Gets or creates the Java counterpart.
  base::android::ScopedJavaLocalRef<jobject> GetJavaObject() override;

  // Delegate for the surface being shown.
  base::WeakPtr<TouchToFillDelegate> delegate_;
  // View that displays the surface, owned by `this`.
  std::unique_ptr<TouchToFillCreditCardView> view_;
  // The corresponding Java TouchToFillCreditCardControllerBridge.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_TOUCH_TO_FILL_PAYMENTS_ANDROID_TOUCH_TO_FILL_CREDIT_CARD_CONTROLLER_H_
