// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_enums.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator_impl.h"
#include "chrome/browser/ui/autofill/chrome_autofill_client.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/common/dense_set.h"
#include "content/public/browser/web_contents_user_data.h"
#include "url/gurl.h"

namespace {
constexpr base::TimeDelta kSleepBetweenTriggerReparseCalls = base::Seconds(1);

constexpr auto kSupportedFormTypes = base::MakeFixedFlatSet<autofill::FormType>(
    {autofill::FormType::kAddressForm, autofill::FormType::kCreditCardForm});

constexpr auto kAddressFieldTypes =
    base::MakeFixedFlatSet<autofill::FieldTypeGroup>(
        {autofill::FieldTypeGroup::kName, autofill::FieldTypeGroup::kEmail,
         autofill::FieldTypeGroup::kPhoneHome,
         autofill::FieldTypeGroup::kAddressHome});

autofill::AutofillField* GetFieldToFill(
    const std::vector<std::unique_ptr<autofill::AutofillField>>& fields,
    bool is_credit_card_form) {
  for (const std::unique_ptr<autofill::AutofillField>& field : fields) {
    if (field->IsFocusable() && field->IsEmpty() &&
        field->IsTextInputElement() &&
        ((!is_credit_card_form &&
          kAddressFieldTypes.contains(field->Type().group())) ||
         (is_credit_card_form &&
          field->Type().GetStorableType() == autofill::CREDIT_CARD_NUMBER))) {
      return field.get();
    }
  }
  return nullptr;
}
}  // namespace

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
  autofill_manager_observation_.Observe(autofill_manager_.get());

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
  // `OnHidden` is not called if the bottom sheet never managed to show,
  // e.g. due to a failed onboarding. This ensures that keyboard suppression
  // stops.
  SetShouldSuppressKeyboard(false);

  // Reset run related state.
  is_running_ = false;
  form_filling_states_.clear();
  form_signatures_to_fill_.clear();
  selected_autofill_profile_.reset();
  selected_credit_card_.reset();
  // Reset UI related state.
  fast_checkout_controller_.reset();
  fast_checkout_ui_state_ = FastCheckoutUIState::kNotShownYet;
  // Reset personal data manager observation.
  personal_data_manager_observation_.Reset();
  // Reset `autofill_manager_` and related objects.
  reparse_timer_.AbandonAndStop();
  autofill_manager_observation_.Reset();
  autofill_manager_.reset();

  if (!allow_further_runs && IsShowing()) {
    fast_checkout_ui_state_ = FastCheckoutUIState::kWasShown;
  }
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
  TryToFillForms();
  autofill_manager_->TriggerReparseInAllFrames(
      base::BindOnce(&FastCheckoutClientImpl::OnTriggerReparseFinished,
                     weak_ptr_factory_.GetWeakPtr()));
}

void FastCheckoutClientImpl::SetFormsToFill() {
  if (!fetcher_) {
    return;
  }
  DCHECK(form_filling_states_.empty());
  DCHECK(form_signatures_to_fill_.empty());
  form_signatures_to_fill_ = fetcher_->GetFormsToFill(origin_);
}

void FastCheckoutClientImpl::OnDismiss() {
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

bool FastCheckoutClientImpl::AllFormsAreFilled() const {
  return base::ranges::all_of(form_filling_states_.begin(),
                              form_filling_states_.end(),
                              [](const auto& pair) {
                                return pair.second == FillingState::kFilled;
                              }) &&
         base::ranges::all_of(
             form_signatures_to_fill_.begin(), form_signatures_to_fill_.end(),
             [&](autofill::FormSignature form_signature) {
               return form_filling_states_.contains(std::make_pair(
                          form_signature, autofill::FormType::kAddressForm)) ||
                      form_filling_states_.contains(std::make_pair(
                          form_signature, autofill::FormType::kCreditCardForm));
             });
}

bool FastCheckoutClientImpl::IsFilling() const {
  return IsRunning() && selected_autofill_profile_ && selected_credit_card_;
}

void FastCheckoutClientImpl::OnAfterLoadedServerPredictions() {
  TryToFillForms();
}

void FastCheckoutClientImpl::OnTriggerReparseFinished(bool success) {
  // `success == true` if `TriggerReparseInAllFrames()` was not called multiple
  // times in parallel, potentially by another actor.
  DCHECK(success);
  if (!reparse_timer_.IsRunning()) {
    // Trigger reparse in all frames continuously until the run stops. That will
    // eventually trigger this (`OnAfterLoadedServerPredictions()`) method.
    reparse_timer_.Start(
        FROM_HERE, kSleepBetweenTriggerReparseCalls,
        base::BindOnce(
            &autofill::AutofillManager::TriggerReparseInAllFrames,
            autofill_manager_,
            base::BindOnce(&FastCheckoutClientImpl::OnTriggerReparseFinished,
                           weak_ptr_factory_.GetWeakPtr())));
  }
}

void FastCheckoutClientImpl::TryToFillForms() {
  if (!IsFilling()) {
    return;
  }
  SetFormFillingStates();
  for (const auto& [_, form] : autofill_manager_->form_structures()) {
    if (ShouldFillForm(*form, autofill::FormType::kAddressForm)) {
      autofill::AutofillField* field =
          GetFieldToFill(form->fields(), /*is_credit_card_form=*/false);
      if (field) {
        form_filling_states_[std::make_pair(form->form_signature(),
                                            autofill::FormType::kAddressForm)] =
            FillingState::kFilling;
        autofill_manager_->FillProfileForm(*selected_autofill_profile_,
                                           form->ToFormData(), *field);
      }
    }

    if (ShouldFillForm(*form, autofill::FormType::kCreditCardForm)) {
      autofill::AutofillField* field =
          GetFieldToFill(form->fields(), /*is_credit_card_form=*/true);
      if (field) {
        form_filling_states_[std::make_pair(
            form->form_signature(), autofill::FormType::kCreditCardForm)] =
            FillingState::kFilling;
        // TODO(crbug.com/1334642): Add CVC popup.
        autofill_manager_->FillCreditCardForm(form->ToFormData(), *field,
                                              *selected_credit_card_, u"");
      }
    }
  }
}

void FastCheckoutClientImpl::SetFormFillingStates() {
  for (const auto& [_, form] : autofill_manager_->form_structures()) {
    // Only attempt to fill forms that were provided by the
    // `FastCheckoutCapabilitiesFetcher`.
    if (!form_signatures_to_fill_.contains(form->form_signature())) {
      continue;
    }
    autofill::DenseSet<autofill::FormType> form_types = form->GetFormTypes();
    for (autofill::FormType form_type : kSupportedFormTypes) {
      // Only attempt to fill forms if they match `form_type`.
      if (!form_types.contains(form_type)) {
        continue;
      }
      auto form_id = std::make_pair(form->form_signature(), form_type);
      if (!form_filling_states_.contains(form_id)) {
        form_filling_states_[form_id] = FillingState::kNotFilled;
      }
    }
  }
}

void FastCheckoutClientImpl::OnAfterDidFillAutofillFormData() {
  if (!IsFilling()) {
    return;
  }
  UpdateFillingStates();
  if (AllFormsAreFilled()) {
    Stop(/*allow_further_runs=*/true);
  }
}

void FastCheckoutClientImpl::UpdateFillingStates() {
  for (auto& [form_id, filling_state] : form_filling_states_) {
    const auto& [form_signature, form_type] = form_id;
    if (form_type == autofill::FormType::kAddressForm &&
        filling_state == FillingState::kFilling) {
      // Assume that if `OnAfterDidFillAutofillFormData()` is called while
      // `this` is in filling mode and there's an address form in `kFilling`
      // state that it got filled.
      filling_state = FillingState::kFilled;
    } else if (form_type == autofill::FormType::kCreditCardForm) {
      auto address_form_id =
          std::make_pair(form_signature, autofill::FormType::kAddressForm);
      if (form_filling_states_.contains(address_form_id) &&
          form_filling_states_[address_form_id] == FillingState::kFilling) {
        // Assume that the address part was filled first if the corresponding
        // form is both an address and a credit card form.
        continue;
      } else if (filling_state == FillingState::kFilling) {
        // Assume that if `OnAfterDidFillAutofillFormData()` is called while
        // `this` is in filling mode and there's a credit card form in
        // `kFilling` state - while no address form of the same signature is in
        // `kFilling` state - that it got filled.
        filling_state = FillingState::kFilled;
      }
    }
  }
}

void FastCheckoutClientImpl::OnAutofillManagerDestroyed() {
  Stop(/*allow_further_runs=*/false);
}

void FastCheckoutClientImpl::OnAutofillManagerReset() {
  if (IsShowing()) {
    Stop(/*allow_further_runs=*/true);
  }
}

bool FastCheckoutClientImpl::ShouldFillForm(
    const autofill::FormStructure& form,
    autofill::FormType expected_form_type) const {
  // Only attempt to fill forms that were provided by the
  // `FastCheckoutCapabilitiesFetcher`.
  if (!form_signatures_to_fill_.contains(form.form_signature())) {
    return false;
  }
  // Only attempt to fill forms if they match `expected_form_type`.
  if (!form.GetFormTypes().contains(expected_form_type)) {
    return false;
  }
  // Attempt to fill forms once only.
  return form_filling_states_.at(
             std::make_pair(form.form_signature(), expected_form_type)) ==
         FillingState::kNotFilled;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);
