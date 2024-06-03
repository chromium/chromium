// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_

#include <memory>

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "content/public/browser/web_contents.h"

// Controller of the bottom sheet surface for filling facilitated payments
// payment methods on Android. It is responsible for showing the view and
// handling user interactions.
class FacilitatedPaymentsController {
 public:
  FacilitatedPaymentsController();

  FacilitatedPaymentsController(const FacilitatedPaymentsController&) = delete;
  FacilitatedPaymentsController& operator=(
      const FacilitatedPaymentsController&) = delete;

  virtual ~FacilitatedPaymentsController();

  // Shows the facilitated payments `view`. Returns whether the surface was
  // successfully shown.
  virtual bool Show(
      std::unique_ptr<
          payments::facilitated::FacilitatedPaymentsBottomSheetBridge> view,
      base::span<const autofill::BankAccount> bank_account_suggestions,
      content::WebContents* web_contents);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

 private:
  // View that displays the surface, owned by `this`.
  std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
      view_;
  // The corresponding Java FacilitatedPaymentsControllerBridge. This bridge is
  // used to delegate user actions from Java to native.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
