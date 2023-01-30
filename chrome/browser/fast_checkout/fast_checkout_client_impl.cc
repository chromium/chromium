// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/metrics/histogram_functions.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_enums.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents),
      autofill_client_(
          autofill::ChromeAutofillClient::FromWebContents(web_contents)),
      fetcher_(FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
          web_contents->GetBrowserContext())),
      personal_data_helper_(
          std::make_unique<FastCheckoutPersonalDataHelperImpl>(web_contents)),
      trigger_validator_(std::make_unique<FastCheckoutTriggerValidatorImpl>(
          autofill_client_,
          fetcher_,
          personal_data_helper_.get())) {}

FastCheckoutClientImpl::~FastCheckoutClientImpl() {
  if (is_running_) {
    base::UmaHistogramEnumeration(kUmaKeyFastCheckoutRunOutcome,
                                  FastCheckoutRunOutcome::kIncompleteRun);
  }
}

bool FastCheckoutClientImpl::TryToStart(
    const GURL& url,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    base::WeakPtr<autofill::AutofillManager> autofill_manager) {
  if (!autofill_manager) {
    return false;
  }

  if (!trigger_validator_->ShouldRun(form, field, fast_checkout_ui_state_,
                                     is_running_, autofill_manager)) {
    return false;
  }

  autofill_manager_ = autofill_manager;
  origin_ = url::Origin::Create(url);
  is_running_ = true;
  personal_data_manager_observation_.Observe(
      personal_data_helper_->GetPersonalDataManager());

  SetFormsToFill();
  SetShouldSuppressKeyboard(true);

  fast_checkout_controller_ = CreateFastCheckoutController();
  ShowFastCheckoutUI();

  fast_checkout_ui_state_ = FastCheckoutUIState::kIsShowing;
  autofill_client_->HideAutofillPopup(
      autofill::PopupHidingReason::kOverlappingWithFastCheckoutSurface);

  return true;
}

void FastCheckoutClientImpl::ShowFastCheckoutUI() {
  fast_checkout_controller_->Show(
      personal_data_helper_->GetProfilesToSuggest(),
      personal_data_helper_->GetCreditCardsToSuggest());
}

void FastCheckoutClientImpl::SetShouldSuppressKeyboard(bool suppress) {
  if (autofill_manager_) {
    autofill_manager_->SetShouldSuppressKeyboard(suppress);
  }
}

void FastCheckoutClientImpl::OnRunComplete() {
  // TODO(crbug.com/1334642): Handle result (e.g. report metrics).
  OnHidden();
  Stop(/*allow_further_runs=*/false);
}

void FastCheckoutClientImpl::Stop(bool allow_further_runs) {
  if (allow_further_runs) {
    forms_to_fill_.clear();
    selected_autofill_profile_.reset();
    selected_credit_card_.reset();
    fast_checkout_ui_state_ = FastCheckoutUIState::kNotShownYet;
  } else if (IsShowing()) {
    fast_checkout_ui_state_ = FastCheckoutUIState::kWasShown;
  }
  fast_checkout_controller_.reset();
  is_running_ = false;
  personal_data_manager_observation_.Reset();

  // `OnHidden` is not called if the bottom sheet never managed to show,
  // e.g. due to a failed onboarding. This ensures that keyboard suppression
  // stops.
  SetShouldSuppressKeyboard(false);

  autofill_manager_ = nullptr;
}

bool FastCheckoutClientImpl::IsShowing() const {
  return fast_checkout_ui_state_ == FastCheckoutUIState::kIsShowing;
}

bool FastCheckoutClientImpl::IsRunning() const {
  return is_running_;
}

std::unique_ptr<FastCheckoutController>
FastCheckoutClientImpl::CreateFastCheckoutController() {
  return std::make_unique<FastCheckoutControllerImpl>(&GetWebContents(), this);
}

void FastCheckoutClientImpl::OnHidden() {
  fast_checkout_ui_state_ = FastCheckoutUIState::kWasShown;
  SetShouldSuppressKeyboard(false);
}

void FastCheckoutClientImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  OnHidden();
  selected_autofill_profile_ = std::move(selected_profile);
  selected_credit_card_ = std::move(selected_credit_card);
}

void FastCheckoutClientImpl::SetFormsToFill() {
  if (!fetcher_) {
    return;
  }
  DCHECK(forms_to_fill_.empty());
  for (autofill::FormSignature form_signature :
       fetcher_->GetFormsToFill(origin_)) {
    forms_to_fill_.emplace(form_signature, FillingState::kNotFilled);
  }
}

void FastCheckoutClientImpl::OnDismiss() {
  OnHidden();
  Stop(/*allow_further_runs=*/false);
  forms_to_fill_.clear();
}

void FastCheckoutClientImpl::OnPersonalDataChanged() {
  if (!IsShowing()) {
    return;
  }

  if (!trigger_validator_->HasValidPersonalData()) {
    Stop(/*allow_further_runs=*/false);
  } else {
    ShowFastCheckoutUI();
  }
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);
