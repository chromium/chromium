// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator_impl.h"

#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/logging/log_macros.h"

using ::autofill::FastCheckoutTriggerOutcome;
using ::autofill::FastCheckoutUIState;

FastCheckoutTriggerValidatorImpl::FastCheckoutTriggerValidatorImpl(
    autofill::AutofillClient* autofill_client,
    FastCheckoutCapabilitiesFetcher* capabilities_fetcher,
    FastCheckoutPersonalDataHelper* personal_data_helper)
    : autofill_client_(autofill_client),
      capabilities_fetcher_(capabilities_fetcher),
      personal_data_helper_(personal_data_helper) {}

FastCheckoutTriggerOutcome FastCheckoutTriggerValidatorImpl::ShouldRun(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const FastCheckoutUIState ui_state,
    const bool is_running,
    const autofill::AutofillManager& autofill_manager) const {
  LogAutofillInternals(
      "Start of checking whether a Fast Checkout run should be permitted.");

  // Trigger only if there is no ongoing run.
  if (is_running) {
    LogAutofillInternals(
        "not triggered because Fast Checkout is already running.");
    return FastCheckoutTriggerOutcome::kUnsupportedFieldType;
  }

  // Trigger only if the URL scheme is cryptographic and security level is not
  // dangerous.
  if (!autofill_client_->IsContextSecure()) {
    LogAutofillInternals(
        "not triggered because context is not secure, e.g. not https or "
        "dangerous security level.");
    return FastCheckoutTriggerOutcome::kUnsupportedFieldType;
  }

  // Trigger only if the form is a trigger form for Fast Checkout.
  if (!IsTriggerForm(form, field)) {
    return FastCheckoutTriggerOutcome::kUnsupportedFieldType;
  }

  if (autofill_client_->GetVariationConfigCountryCode() !=
      GeoIpCountryCode("US")) {
    return FastCheckoutTriggerOutcome::kUnsupportedCountry;
  }

  // UMA drop out metrics are recorded after this point only to avoid collecting
  // unnecessary metrics that would dominate the other data points.
  // Trigger only if not shown before.
  if (ui_state != FastCheckoutUIState::kNotShownYet) {
    LogAutofillInternals("not triggered because it was shown before.");
    return FastCheckoutTriggerOutcome::kFailureShownBefore;
  }

  // Trigger only on focusable fields.
  if (!field.is_focusable()) {
    LogAutofillInternals("not triggered because field was not focusable.");
    return FastCheckoutTriggerOutcome::kFailureFieldNotFocusable;
  }

  // Trigger only on empty fields.
  // TODO: crbug.com/40227496 - `field.value()` is ambiguous: sometimes it's the
  // current value, sometimes it's the initial value. The reason is that some
  // callers upcast `field` from `AutofillField`, whose `value()` is the initial
  // value. The feature `kAutofillFixValueSemantics` fixes this.
  if (!field.value().empty()) {
    LogAutofillInternals("not triggered because field was not empty.");
    return FastCheckoutTriggerOutcome::kFailureFieldNotEmpty;
  }

  // Trigger only if the UI is available.
  if (!autofill_manager.CanShowAutofillUi()) {
    LogAutofillInternals("not triggered because Autofill UI cannot be shown.");
    return FastCheckoutTriggerOutcome::kFailureCannotShowAutofillUi;
  }

  FastCheckoutTriggerOutcome result = HasValidPersonalData();
  if (result != FastCheckoutTriggerOutcome::kSuccess) {
    return result;
  }

  LogAutofillInternals("was triggered successfully.");
  return FastCheckoutTriggerOutcome::kSuccess;
}

bool FastCheckoutTriggerValidatorImpl::IsTriggerForm(
    const autofill::FormData& form,
    const autofill::FormFieldData& field) const {
  if (!capabilities_fetcher_) {
    return false;
  }
  // TODO(crbug.com/40236321): Stop calculating the signature once the form
  // signature has been moved to `form_data`.
  // Check browser form's signature and renderer form's signature.
  autofill::FormSignature form_signature =
      autofill::CalculateFormSignature(form);
  bool is_trigger_form =
      capabilities_fetcher_->IsTriggerFormSupported(form.main_frame_origin(),
                                                    form_signature) ||
      capabilities_fetcher_->IsTriggerFormSupported(
          form.main_frame_origin(), field.host_form_signature());
  if (!is_trigger_form) {
    LogAutofillInternals(
        "not triggered because there is no Fast Checkout support for form "
        "signatures {" +
        base::NumberToString(form_signature.value()) + ", " +
        base::NumberToString(field.host_form_signature().value()) +
        "} on origin " + form.main_frame_origin().Serialize() + ".");
  }
  return is_trigger_form;
}

FastCheckoutTriggerOutcome
FastCheckoutTriggerValidatorImpl::HasValidPersonalData() const {
  autofill::PersonalDataManager* pdm =
      personal_data_helper_->GetPersonalDataManager();
  if (!pdm->address_data_manager().IsAutofillProfileEnabled()) {
    LogAutofillInternals("not triggered because Autofill profile is disabled.");
    return FastCheckoutTriggerOutcome::kFailureAutofillProfileDisabled;
  }

  if (!pdm->payments_data_manager().IsAutofillPaymentMethodsEnabled()) {
    LogAutofillInternals(
        "not triggered because Autofill credit card is disabled.");
    return FastCheckoutTriggerOutcome::kFailureAutofillCreditCardDisabled;
  }

  // Trigger only if there is at least 1 valid Autofill profile on file.
  if (personal_data_helper_->GetValidAddressProfiles().empty()) {
    LogAutofillInternals(
        "not triggered because the client does not have at least one valid "
        "Autofill profile stored.");
    return FastCheckoutTriggerOutcome::kFailureNoValidAutofillProfile;
  }

  // Trigger only if there is at least 1 complete valid credit card on file.
  if (personal_data_helper_->GetValidCreditCards().empty()) {
    LogAutofillInternals(
        "not triggered because the client does not have at least one "
        "valid Autofill credit card stored.");
    return FastCheckoutTriggerOutcome::kFailureNoValidCreditCard;
  }

  return FastCheckoutTriggerOutcome::kSuccess;
}

void FastCheckoutTriggerValidatorImpl::LogAutofillInternals(
    std::string message) const {
  LOG_AF(autofill_client_->GetLogManager())
      << autofill::LoggingScope::kFastCheckout
      << autofill::LogMessage::kFastCheckout << message;
}
