// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"

#include "chrome/browser/fast_checkout/fast_checkout_util.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill_assistant/browser/public/fast_checkout/proto/actions.pb.h"

namespace {
// Note: the value of `kProfileName` must be in sync with Autofill Assistant's
// server version.
constexpr char kProfileName[] = "SHIPPING";
}  // namespace

FastCheckoutExternalActionDelegate::FastCheckoutExternalActionDelegate() =
    default;

FastCheckoutExternalActionDelegate::~FastCheckoutExternalActionDelegate() =
    default;

void FastCheckoutExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const autofill_assistant::external::Result&)>
        end_action_callback) {
  if (!action.info().has_fast_checkout_action()) {
    DLOG(ERROR) << "Action is not of type FastCheckoutAction";
    CancelInvalidActionRequest(std::move(end_action_callback));
    return;
  }

  autofill_assistant::fast_checkout::FastCheckoutAction fast_checkout_action =
      action.info().fast_checkout_action();

  switch (fast_checkout_action.action_case()) {
    case autofill_assistant::fast_checkout::FastCheckoutAction::ActionCase::
        kWaitForUserSelection:
      // Waits for user selection of address and credit card and communicates
      // it back to external action via callback.
      wait_for_user_selection_action_callback_ = std::move(end_action_callback);
      if (selected_profile_proto_ && selected_credit_card_proto_)
        EndWaitForUserSelectionAction();
      break;
    case autofill_assistant::fast_checkout::FastCheckoutAction::ActionCase::
        ACTION_NOT_SET:
      DLOG(ERROR) << "unknown fast checkout action";
      CancelInvalidActionRequest(std::move(end_action_callback));
      break;
  }
}

void FastCheckoutExternalActionDelegate::SetOptionsSelected(
    const autofill::AutofillProfile& selected_profile,
    const autofill::CreditCard& selected_credit_card) {
  selected_profile_proto_ = fast_checkout::CreateProfileProto(selected_profile);
  selected_credit_card_proto_ =
      fast_checkout::CreateCreditCardProto(selected_credit_card);

  if (wait_for_user_selection_action_callback_)
    EndWaitForUserSelectionAction();
}

void FastCheckoutExternalActionDelegate::EndWaitForUserSelectionAction() {
  autofill_assistant::external::Result result;
  result.set_success(true);

  result.mutable_selected_profiles()->insert(
      {kProfileName, selected_profile_proto_.value()});
  *result.mutable_selected_credit_card() = selected_credit_card_proto_.value();

  std::move(wait_for_user_selection_action_callback_).Run(std::move(result));
}

void FastCheckoutExternalActionDelegate::CancelInvalidActionRequest(
    base::OnceCallback<void(const autofill_assistant::external::Result&)>
        end_action_callback) {
  autofill_assistant::external::Result result;
  result.set_success(false);
  std::move(end_action_callback).Run(std::move(result));
}
