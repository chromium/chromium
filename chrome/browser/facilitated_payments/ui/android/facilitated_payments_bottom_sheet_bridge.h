// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_

#include <jni.h>

#include "base/android/scoped_java_ref.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
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
  FacilitatedPaymentsBottomSheetBridge(
      content::WebContents* web_contents,
      FacilitatedPaymentsController* controller);

  FacilitatedPaymentsBottomSheetBridge(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;
  FacilitatedPaymentsBottomSheetBridge& operator=(
      const FacilitatedPaymentsBottomSheetBridge&) = delete;

  virtual ~FacilitatedPaymentsBottomSheetBridge();

  // Returns true if the device is being used in the landscape mode.
  virtual bool IsInLandscapeMode();

  // Show the payment prompt containing user's `bank_account_suggestions`.
  // Return true if a new bottom sheet is created and shown. Otherwise, return
  // false.
  virtual bool RequestShowContent(
      base::span<const autofill::BankAccount> bank_account_suggestions);

  // Triggers showing the progress screen. Virtual for overriding in tests.
  virtual void ShowProgressScreen();

  // Triggers showing the error screen. Virtual for overriding in tests.
  virtual void ShowErrorScreen();

  // Closes the bottom sheet. Virtual for overriding in tests.
  virtual void Dismiss();

  // Called whenever the surface gets hidden (regardless of the cause). Virtual
  // for testing.
  virtual void OnDismissed();

 private:
  // Lazily initializes and returns `java_bridge_`;
  base::android::ScopedJavaLocalRef<jobject> GetJavaBridge();

  base::WeakPtr<content::WebContents> web_contents_;
  // Owner.
  raw_ptr<FacilitatedPaymentsController> controller_;
  // The corresponding Java FacilitatedPaymentsPaymentMethodsViewBridge. This
  // bridge is used to pass info and commands from native side to Java side for
  // showing UI prompts.
  base::android::ScopedJavaGlobalRef<jobject> java_bridge_;
};

}  // namespace payments::facilitated

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_BOTTOM_SHEET_BRIDGE_H_
