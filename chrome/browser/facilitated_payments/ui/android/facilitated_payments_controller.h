// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_

#include <memory>

#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "components/autofill/core/browser/data_model/bank_account.h"

namespace content {
class WebContents;
}

// Controller of the bottom sheet surface for filling facilitated payments
// payment methods on Android. It is responsible for showing the view and
// handling user interactions.
class FacilitatedPaymentsController {
 public:
  explicit FacilitatedPaymentsController(content::WebContents* web_contents);

  FacilitatedPaymentsController(const FacilitatedPaymentsController&) = delete;
  FacilitatedPaymentsController& operator=(
      const FacilitatedPaymentsController&) = delete;

  virtual ~FacilitatedPaymentsController();

  // Returns true if the device is being used in the landscape mode.
  virtual bool IsInLandscapeMode();

  // Asks the `view_` to show the FOP selector. Returns whether the surface was
  // successfully shown.
  virtual bool Show(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Asks the `view_` to show the progress screen. Virtual for overriding in
  // tests.
  virtual void ShowProgressScreen();

  // Asks the `view_` to show the error screen. Virtual for overriding in tests.
  virtual void ShowErrorScreen();

  // Asks the `view_` to close the bottom sheet. Virtual for overriding in
  // tests.
  virtual void Dismiss();

  // Called whenever the surface gets hidden (regardless of the cause).
  virtual void OnDismissed(JNIEnv* env);

  void OnBankAccountSelected(JNIEnv* env, jlong instrument_id);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void SetViewForTesting(
      std::unique_ptr<
          payments::facilitated::FacilitatedPaymentsBottomSheetBridge> view);

 private:
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsControllerTest, OnDismissed);

  // View that displays the surface.
  std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
      view_;
  // The corresponding Java FacilitatedPaymentsControllerBridge. This bridge is
  // used to delegate user actions from Java to native.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;
  // Called after showing PIX payment prompt.
  base::OnceCallback<void(bool, int64_t)> on_user_decision_callback_;
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
