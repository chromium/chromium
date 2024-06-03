// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "ui/android/window_android.h"

class FacilitatedPaymentsController;

namespace content {
class WebContents;
}  // namespace content

namespace payments::facilitated {

// Bridge class providing an entry point to trigger the facilitated payments
// bottom sheet on Android.
class FacilitatedPaymentsBottomSheetBridge {
 public:
  FacilitatedPaymentsBottomSheetBridge();

  FacilitatedPaymentsBottomSheetBridge(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;
  FacilitatedPaymentsBottomSheetBridge& operator=(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;

  virtual ~FacilitatedPaymentsBottomSheetBridge();

  // Show the payment prompt containing user's `bank_account_suggestions`.
  // Return true if a new bottom sheet is created and shown. Otherwise, return
  // false.
  virtual bool RequestShowContent(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      FacilitatedPaymentsController* controller,
      content::WebContents* web_contents);

 private:
  // The corresponding Java FacilitatedPaymentsPaymentMethodsViewBridge. This
  // bridge is used to pass info and commands from native side to Java side for
  // showing UI prompts.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace payments::facilitated

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
