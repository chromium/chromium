// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_

#include "components/autofill_assistant/browser/public/autofill_assistant.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace autofill {
class AutofillProfile;
class CreditCard;
}  // namespace autofill

// Handles external actions defined for fast checkout.
class FastCheckoutExternalActionDelegate
    : public autofill_assistant::ExternalActionDelegate {
 public:
  FastCheckoutExternalActionDelegate();
  ~FastCheckoutExternalActionDelegate() override;

  FastCheckoutExternalActionDelegate(
      const FastCheckoutExternalActionDelegate&) = delete;
  FastCheckoutExternalActionDelegate& operator=(
      const FastCheckoutExternalActionDelegate&) = delete;

  // Saves user selections and sends selections back to external action via
  // callback if wait for user selection action is already running.
  virtual void SetOptionsSelected(
      const autofill::AutofillProfile& selected_profile,
      const autofill::CreditCard& selected_credit_card);

  // ExternalActionDelegate:
  void OnActionRequested(
      const autofill_assistant::external::Action& action,
      base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
      base::OnceCallback<void(const autofill_assistant::external::Result&)>
          end_action_callback) override;

 private:
  // Ends the current action by notifying the `ExternalActionController`.
  void EndWaitForUserSelectionAction();

  // Ends the current action request because the action was not recognized by
  // `this`.
  void CancelInvalidActionRequest(
      base::OnceCallback<void(const autofill_assistant::external::Result&)>
          end_action_callback);

  // The callback that terminates the current action.
  base::OnceCallback<void(const autofill_assistant::external::Result& result)>
      wait_for_user_selection_action_callback_;

  // Proto representation of the Autofill profile selected by the user.
  absl::optional<autofill_assistant::external::ProfileProto>
      selected_profile_proto_;

  // Proto representation of the credit card selected by the user.
  absl::optional<autofill_assistant::external::CreditCardProto>
      selected_credit_card_proto_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_EXTERNAL_ACTION_DELEGATE_H_
