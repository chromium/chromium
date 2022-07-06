// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_external_action_delegate.h"

#include "chrome/browser/fast_checkout/fast_checkout_util.h"
#include "chrome/browser/fast_checkout/proto/fast_checkout.pb.h"
#include "chrome/browser/ui/fast_checkout/fast_checkout_controller_impl.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"

namespace {
// Note: the value of `kProfileName` must be in sync with Autofill Assistant's
// server version.
constexpr char kProfileName[] = "SHIPPING";
}  // namespace

FastCheckoutExternalActionDelegate::FastCheckoutExternalActionDelegate(
    content::WebContents* web_contents)
    : fast_checkout_controller_(
          std::make_unique<FastCheckoutControllerImpl>(web_contents, this)) {}

FastCheckoutExternalActionDelegate::~FastCheckoutExternalActionDelegate() =
    default;

void FastCheckoutExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const autofill_assistant::external::Result&)>
        end_action_callback) {
  FastCheckoutAction fast_checkout_action;
  if (!fast_checkout_action.ParseFromString(action.info().action_payload())) {
    DLOG(ERROR) << "unable to parse FastCheckoutAction";
    CancelInvalidActionRequest(std::move(end_action_callback));
    return;
  }

  switch (fast_checkout_action.action_case()) {
    case FastCheckoutAction::ActionCase::kShowBottomSheet:
      // Show bottomsheet UI.
      end_show_bottomsheet_action_callback_ = std::move(end_action_callback);
      fast_checkout_controller_->Show();
      break;
    case FastCheckoutAction::ActionCase::ACTION_NOT_SET:
      DLOG(ERROR) << "unknown fast checkout action";
      CancelInvalidActionRequest(std::move(end_action_callback));
      break;
  }
}

void FastCheckoutExternalActionDelegate::OnInterruptStarted() {
  // Currently interrupts are not required for this.
  // TODO(crrev.com/c/3734869): Remove once linked CL is merged.
}

void FastCheckoutExternalActionDelegate::OnInterruptFinished() {
  // Currently interrupts are not required for this.
  // TODO(crrev.com/c/3734869): Remove once linked CL is merged.
}

void FastCheckoutExternalActionDelegate::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  if (selected_profile && selected_credit_card) {
    EndShowBottomSheetAction(true, std::move(selected_profile),
                             std::move(selected_credit_card));
    return;
  }

  // Should not be reached.
  DLOG(ERROR) << "FastCheckoutExternalActionDelegate::OnOptionsSelected was "
                 "passed at least one null pointer.";
  EndShowBottomSheetAction(false);
}

void FastCheckoutExternalActionDelegate::OnDismiss() {
  EndShowBottomSheetAction(false);
}

void FastCheckoutExternalActionDelegate::EndShowBottomSheetAction(
    bool success,
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  autofill_assistant::external::Result result;
  result.set_success(success);

  if (success) {
    result.mutable_selected_profiles()->insert(
        {kProfileName, fast_checkout::CreateProfileProto(*selected_profile)});
    *result.mutable_selected_credit_card() =
        fast_checkout::CreateCreditCardProto(*selected_credit_card);
  }

  std::move(end_show_bottomsheet_action_callback_).Run(std::move(result));
}

void FastCheckoutExternalActionDelegate::CancelInvalidActionRequest(
    base::OnceCallback<void(const autofill_assistant::external::Result&)>
        end_action_callback) {
  autofill_assistant::external::Result result;
  result.set_success(false);
  std::move(end_action_callback).Run(std::move(result));
}
