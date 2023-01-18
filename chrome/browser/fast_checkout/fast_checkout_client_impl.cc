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
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

FastCheckoutClientImpl::FastCheckoutClientImpl(
    content::WebContents* web_contents)
    : content::WebContentsUserData<FastCheckoutClientImpl>(*web_contents),
      autofill_client_(
          autofill::ChromeAutofillClient::FromWebContents(web_contents)),
      personal_data_helper_(
          std::make_unique<FastCheckoutPersonalDataHelperImpl>(web_contents)),
      trigger_validator_(std::make_unique<FastCheckoutTriggerValidatorImpl>(
          autofill_client_,
          FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
              web_contents->GetBrowserContext()),
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
    autofill::AutofillDriver* autofill_driver) {
  autofill::ContentAutofillDriver* content_autofill_driver =
      static_cast<autofill::ContentAutofillDriver*>(autofill_driver);

  if (!content_autofill_driver) {
    return false;
  }

  if (!trigger_validator_->ShouldRun(form, field, fast_checkout_ui_state_,
                                     is_running_, content_autofill_driver)) {
    return false;
  }

  autofill_driver_ = content_autofill_driver;
  url_ = url;
  is_running_ = true;
  personal_data_manager_observation_.Observe(
      personal_data_helper_->GetPersonalDataManager());

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
  if (autofill_driver_) {
    autofill_driver_->SetShouldSuppressKeyboard(suppress);
  }
}

void FastCheckoutClientImpl::OnRunComplete() {
  // TODO(crbug.com/1334642): Handle result (e.g. report metrics).
  OnHidden();
  Stop(/*allow_further_runs=*/false);
}

void FastCheckoutClientImpl::Stop(bool allow_further_runs) {
  if (allow_further_runs) {
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

  // There is one `ContentAutofillDriver` instance per frame but only one
  // instance of this class per `WebContents`. Reset `autofill_driver_` here to
  // avoid the issue of having a non-null, invalid pointer. This method is
  // (also) called from `~BrowserAutofillManager()` which is owned by
  // `ContentAutofillDriver`.
  autofill_driver_ = nullptr;
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
  // TODO(crbug.com/1334642): Signal that FC options have been selected.
  OnHidden();
}

void FastCheckoutClientImpl::OnDismiss() {
  OnHidden();
  Stop(/*allow_further_runs=*/false);
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
