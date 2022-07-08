// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_

#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "content/public/browser/web_contents.h"

namespace content {
class WebContents;
}  // namespace content

// Handles external actions defined for fast checkout.
class FastCheckoutExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate,
      public FastCheckoutControllerImpl::Delegate {
 public:
  explicit FastCheckoutExternalActionDelegate(
      content::WebContents* web_contents);
  ~FastCheckoutExternalActionDelegate() override;

  FastCheckoutExternalActionDelegate(
      const FastCheckoutExternalActionDelegate&) = delete;
  FastCheckoutExternalActionDelegate& operator=(
      const FastCheckoutExternalActionDelegate&) = delete;

  // ExternalActionDelegate:
  void OnActionRequested(
      const autofill_assistant::external::Action& action,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&)>
          end_action_callback) override;

  // FastCheckoutControllerImpl::Delegate:
  void OnOptionsSelected(
      std::unique_ptr<autofill::AutofillProfile> profile,
      std::unique_ptr<autofill::CreditCard> credit_card) override;
  void OnDismiss() override;

#ifdef UNIT_TEST
  void SetFastCheckoutControllerForTest(
      std::unique_ptr<FastCheckoutController> controller) {
    fast_checkout_controller_ = std::move(controller);
  }
#endif

 private:
  // Ends the current action by notifying the `ExternalActionController` about
  // the `success` of the action. If existent, `selected_profile` and
  // `selected_credit_card` are set in the `Result` proto.
  void EndShowBottomSheetAction(
      bool success,
      std::unique_ptr<autofill::AutofillProfile> selected_profile = nullptr,
      std::unique_ptr<autofill::CreditCard> selected_credit_card = nullptr);

  // Ends the current action request because the action was not recognized by
  // `this`.
  void CancelInvalidActionRequest(
      base::OnceCallback<void(const autofill_assistant::external::Result&)>
          end_action_callback);

  // The callback that terminates the current action.
  base::OnceCallback<void(const autofill_assistant::external::Result& result)>
      end_show_bottomsheet_action_callback_;

  // Fast Checkout bottom sheet UI controller.
  std::unique_ptr<FastCheckoutController> fast_checkout_controller_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
