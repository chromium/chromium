// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
#define CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_

#include <memory>

#include "base/containers/span.h"
#include "base/functional/callback_forward.h"
#include "chrome/browser/facilitated_payments/ui/android/facilitated_payments_bottom_sheet_bridge.h"
#include "components/autofill/core/browser/data_model/bank_account.h"
#include "components/autofill/core/browser/data_model/ewallet.h"
#include "components/facilitated_payments/core/ui_utils/facilitated_payments_ui_utils.h"

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

  // Shows the PIX FOP selector.
  virtual void Show(
      base::span<const autofill::BankAccount> bank_account_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Shows the eWallet FOP selector.
  virtual void ShowForEwallet(
      base::span<const autofill::Ewallet> ewallet_suggestions,
      base::OnceCallback<void(bool, int64_t)> on_user_decision_callback);

  // Asks the `view_` to show the progress screen. Virtual for overriding in
  // tests.
  virtual void ShowProgressScreen();

  // Asks the `view_` to show the error screen. Virtual for overriding in tests.
  virtual void ShowErrorScreen();

  // Asks the `view_` to close the bottom sheet. Virtual for overriding in
  // tests.
  virtual void Dismiss();

  // Enables features to listen to `payments::facilitated::UiEvent` using
  // `ui_event_listener`.
  void SetUiEventListener(
      base::RepeatingCallback<void(payments::facilitated::UiEvent)>
          ui_event_listener);

  // Called by the Java view to communicate `payments::facilitated::UiEvent`.
  void OnUiEvent(JNIEnv* env, jint event);

  // Called whenever the surface gets hidden (regardless of the cause).
  virtual void OnDismissed(JNIEnv* env);

  void OnBankAccountSelected(JNIEnv* env, jlong instrument_id);

  void OnEwalletSelected(JNIEnv* env, jlong instrument_id);

  base::android::ScopedJavaLocalRef<jobject> GetJavaObject();

  void SetViewForTesting(
      std::unique_ptr<
          payments::facilitated::FacilitatedPaymentsBottomSheetBridge> view);

 private:
  FRIEND_TEST_ALL_PREFIXES(FacilitatedPaymentsControllerTest, OnDismissed);

  // Clears any native references from the Java view components, and then clears
  // the pointers to the Java objects.
  void ClearJavaViewComponents();

  // View that displays the surface.
  std::unique_ptr<payments::facilitated::FacilitatedPaymentsBottomSheetBridge>
      view_;

  // The corresponding Java FacilitatedPaymentsControllerBridge. This bridge is
  // used to delegate user actions from Java to native.
  base::android::ScopedJavaGlobalRef<jobject> java_object_;

  // TODO: crbug.com/375089558 - Make this on_user_selected_ when user decline
  // can be routed using ui_event_listener_.
  // Called after showing PIX payment prompt.
  base::OnceCallback<void(bool, int64_t)> on_user_decision_callback_;

  // Callback used to communicate view events to the feature.
  base::RepeatingCallback<void(payments::facilitated::UiEvent)>
      ui_event_listener_;
};

#endif  // CHROME_BROWSER_FACILITATED_PAYMENTS_UI_ANDROID_FACILITATED_PAYMENTS_CONTROLLER_H_
