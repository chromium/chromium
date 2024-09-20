// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/payments_suggestion_generator.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/ui/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"

namespace autofill {

namespace {

// Checks if the field is focusable and empty.
bool IsFieldFocusableAndEmpty(const FormData& received_form,
                              FieldGlobalId field_id) {
  // form_field->value extracted from FormData represents field's *current*
  // value, not the original value.
  const FormFieldData* form_field = received_form.FindFieldByGlobalId(field_id);
  return form_field && form_field->IsFocusable() &&
         SanitizedFieldIsEmpty(form_field->value());
}

bool IsTriggeredOnIbanField(const FormStructure* form_field,
                            const FormFieldData& field) {
  if (!form_field) {
    return false;
  }

  const autofill::AutofillField* autofill_field =
      form_field->GetFieldById(field.global_id());
  return autofill_field &&
         autofill_field->Type().group() == FieldTypeGroup::kIban;
}

}  // namespace

TouchToFillDelegateAndroidImpl::DryRunResult::DryRunResult(
    TriggerOutcome outcome,
    absl::variant<std::vector<CreditCard>, std::vector<Iban>> items_to_suggest)
    : outcome(outcome), items_to_suggest(std::move(items_to_suggest)) {}

TouchToFillDelegateAndroidImpl::DryRunResult::DryRunResult(DryRunResult&&) =
    default;

TouchToFillDelegateAndroidImpl::DryRunResult&
TouchToFillDelegateAndroidImpl::DryRunResult::operator=(DryRunResult&&) =
    default;

TouchToFillDelegateAndroidImpl::DryRunResult::~DryRunResult() = default;

TouchToFillDelegateAndroidImpl::TouchToFillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(manager) {
  DCHECK(manager);
}

TouchToFillDelegateAndroidImpl::~TouchToFillDelegateAndroidImpl() {
  // Invalidate pointers to avoid post hide callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  HideTouchToFill();
}

// TODO(crbug.com/40282650): Remove received FormData
TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRun(FormGlobalId form_id,
                                       FieldGlobalId field_id,
                                       const FormData& received_form) {
  // Trigger only on supported platforms.
  if (!IsTouchToFillPaymentMethodSupported()) {
    return {TriggerOutcome::kUnsupportedFieldType, {}};
  }
  const FormStructure* form = manager_->FindCachedFormById(form_id);
  if (!form) {
    return {TriggerOutcome::kUnknownForm, {}};
  }
  const AutofillField* field = form->GetFieldById(field_id);
  if (!field) {
    return {TriggerOutcome::kUnknownField, {}};
  }
  // Trigger only if not shown before.
  if (ttf_payment_method_state_ != TouchToFillState::kShouldShow) {
    return {TriggerOutcome::kShownBefore, {}};
  }
  // Trigger only if the client and the form are not insecure.
  if (IsFormOrClientNonSecure(manager_->client(), *form)) {
    return {TriggerOutcome::kFormOrClientNotSecure, {}};
  }
  // Trigger only on focusable empty field.
  if (!IsFieldFocusableAndEmpty(received_form, field_id)) {
    return {TriggerOutcome::kFieldNotEmptyOrNotFocusable, {}};
  }
  // Trigger only if the UI is available.
  if (!manager_->CanShowAutofillUi()) {
    return {TriggerOutcome::kCannotShowAutofillUi, {}};
  }

  if (field->Type().group() == FieldTypeGroup::kIban) {
    return DryRunForIban();
  } else if (field->Type().group() == FieldTypeGroup::kCreditCard) {
    return DryRunForCreditCard(*field, *form, received_form);
  }

  return {TriggerOutcome::kUnsupportedFieldType, {}};
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRunForIban() {
  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  CHECK(pdm);
  std::vector<Iban> ibans_to_suggest =
      pdm->payments_data_manager().GetOrderedIbansToSuggest();
  return ibans_to_suggest.empty() || !base::FeatureList::IsEnabled(
                                         features::kAutofillEnableLocalIban)
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(ibans_to_suggest));
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRunForCreditCard(
    const AutofillField& field,
    const FormStructure& form,
    const FormData& received_form) {
  // Trigger only for complete forms (containing the fields for the card number
  // and the card expiration date).
  if (!FormHasAllCreditCardFields(form)) {
    return {TriggerOutcome::kIncompleteForm, {}};
  }
  if (IsFormPrefilled(received_form)) {
    return {TriggerOutcome::kFormAlreadyFilled, {}};
  }
  // Trigger only if Fast Checkout was not shown before.
  if (!manager_->client().GetFastCheckoutClient()->IsNotShownYet()) {
    return {TriggerOutcome::kFastCheckoutWasShown, {}};
  }

  // Fetch all complete valid credit cards on file.
  // Complete = contains number, expiration date and name on card.
  // Valid = unexpired with valid number format.
  // TODO(crbug.com/40227496): `*field` must contain the updated field
  // information.
  std::vector<CreditCard> cards_to_suggest = GetTouchToFillCardsToSuggest(
      manager_->client(), field, field.Type().GetStorableType());
  return cards_to_suggest.empty()
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(cards_to_suggest));
}

// TODO(crbug.com/40282650): Remove received FormData
bool TouchToFillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id,
    const FormData& form) {
  TriggerOutcome outcome = DryRun(form_id, field_id, form).outcome;
  LOG_AF(manager_->client().GetLogManager())
      << LoggingScope::kTouchToFill << LogMessage::kTouchToFill
      << "dry run before parsing for form " << form_id << " and field "
      << field_id << " was " << (outcome == TriggerOutcome::kShown ? "" : "un")
      << "successful (" << base::to_underlying(outcome) << ")";
  return outcome == TriggerOutcome::kShown;
}

bool TouchToFillDelegateAndroidImpl::TryToShowTouchToFill(
    const FormData& form,
    const FormFieldData& field) {
  // TODO(crbug.com/40247130): store only FormGlobalId and FieldGlobalId instead
  // to avoid that FormData and FormFieldData may become obsolete during the
  // bottomsheet being open.
  query_form_ = form;
  query_field_ = field;
  DryRunResult dry_run = DryRun(form.global_id(), field.global_id(), form);
  if (dry_run.outcome == TriggerOutcome::kShown) {
    if (std::vector<CreditCard>* cards_to_suggest =
            absl::get_if<std::vector<CreditCard>>(&dry_run.items_to_suggest);
        cards_to_suggest &&
        !manager_->client()
             .GetPaymentsAutofillClient()
             ->ShowTouchToFillCreditCard(
                 GetWeakPtr(), *cards_to_suggest,
                 GetCreditCardSuggestionsForTouchToFill(*cards_to_suggest,
                                                        manager_->client()))) {
      dry_run.outcome = TriggerOutcome::kFailedToDisplayBottomSheet;
    } else if (std::vector<Iban>* ibans_to_suggest =
                   absl::get_if<std::vector<Iban>>(&dry_run.items_to_suggest);
               ibans_to_suggest &&
               (base::FeatureList::IsEnabled(
                    features::kAutofillSkipAndroidBottomSheetForIban) ||
                !manager_->client()
                     .GetPaymentsAutofillClient()
                     ->ShowTouchToFillIban(GetWeakPtr(),
                                           std::move(*ibans_to_suggest)))) {
      dry_run.outcome = TriggerOutcome::kFailedToDisplayBottomSheet;
    }
  }

  if (dry_run.outcome != TriggerOutcome::kUnsupportedFieldType) {
    if (IsTriggeredOnIbanField(manager_->FindCachedFormById(form.global_id()),
                               field)) {
      base::UmaHistogramEnumeration(kUmaTouchToFillIbanTriggerOutcome,
                                    dry_run.outcome);
    } else {
      base::UmaHistogramEnumeration(kUmaTouchToFillCreditCardTriggerOutcome,
                                    dry_run.outcome);
    }
  }
  LOG_AF(manager_->client().GetLogManager())
      << LoggingScope::kTouchToFill << LogMessage::kTouchToFill
      << "dry run after parsing for form " << form.global_id() << " and field "
      << field.global_id() << " was "
      << (dry_run.outcome == TriggerOutcome::kShown ? "" : "un")
      << "successful (" << base::to_underlying(dry_run.outcome) << ")";
  if (dry_run.outcome != TriggerOutcome::kShown) {
    return false;
  }

  ttf_payment_method_state_ = TouchToFillState::kIsShowing;
  manager_->client().HideAutofillSuggestions(
      SuggestionHidingReason::kOverlappingWithTouchToFillSurface);
  if (absl::get_if<std::vector<CreditCard>>(&dry_run.items_to_suggest)) {
    manager_->DidShowSuggestions({SuggestionType::kCreditCardEntry}, form,
                                 field);
  } else {
    manager_->DidShowSuggestions({SuggestionType::kIbanEntry}, form, field);
  }
  return true;
}

bool TouchToFillDelegateAndroidImpl::IsShowingTouchToFill() {
  return ttf_payment_method_state_ == TouchToFillState::kIsShowing;
}

// TODO(crbug.com/40233391): Create a central point for TTF hiding decision.
void TouchToFillDelegateAndroidImpl::HideTouchToFill() {
  if (IsShowingTouchToFill()) {
    manager_->client()
        .GetPaymentsAutofillClient()
        ->HideTouchToFillPaymentMethod();
  }
}

void TouchToFillDelegateAndroidImpl::Reset() {
  HideTouchToFill();
  ttf_payment_method_state_ = TouchToFillState::kShouldShow;
}

AutofillManager* TouchToFillDelegateAndroidImpl::GetManager() {
  return manager_;
}

bool TouchToFillDelegateAndroidImpl::ShouldShowScanCreditCard() {
  if (!manager_->client()
           .GetPaymentsAutofillClient()
           ->HasCreditCardScanFeature()) {
    return false;
  }

  return !IsFormOrClientNonSecure(manager_->client(), query_form_);
}

void TouchToFillDelegateAndroidImpl::ScanCreditCard() {
  manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(base::BindOnce(
      &TouchToFillDelegateAndroidImpl::OnCreditCardScanned, GetWeakPtr()));
}

void TouchToFillDelegateAndroidImpl::OnCreditCardScanned(
    const CreditCard& card) {
  HideTouchToFill();
  manager_->FillOrPreviewCreditCardForm(
      mojom::ActionPersistence::kFill, query_form_, query_field_, card,
      std::u16string(),
      {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
}

void TouchToFillDelegateAndroidImpl::ShowPaymentMethodSettings() {
  manager_->client().ShowAutofillSettings(SuggestionType::kManageCreditCard);
}

void TouchToFillDelegateAndroidImpl::CreditCardSuggestionSelected(
    std::string unique_id,
    bool is_virtual) {
  HideTouchToFill();

  PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
  CHECK(pdm);
  const CreditCard* card =
      pdm->payments_data_manager().GetCreditCardByGUID(unique_id);
  // TODO(crbug.com/40071928): Figure out why `card` is sometimes nullptr.
  if (!card) {
    return;
  }
  if (is_virtual) {
    // Virtual credit cards are not persisted in Chrome, modify record type
    // locally.
    manager_->AuthenticateThenFillCreditCardForm(
        query_form_, query_field_, CreditCard::CreateVirtualCard(*card),
        {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
  } else {
    manager_->AuthenticateThenFillCreditCardForm(
        query_form_, query_field_, *card,
        {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
  }
}

void TouchToFillDelegateAndroidImpl::IbanSuggestionSelected(
    absl::variant<Iban::Guid, Iban::InstrumentId> backend_id) {
  HideTouchToFill();

  manager_->client()
      .GetPaymentsAutofillClient()
      ->GetIbanAccessManager()
      ->FetchValue(
          absl::holds_alternative<Iban::Guid>(backend_id)
              ? Suggestion::BackendId(
                    Suggestion::Guid(absl::get<Iban::Guid>(backend_id).value()))
              : Suggestion::BackendId(Suggestion::InstrumentId(
                    absl::get<Iban::InstrumentId>(backend_id).value())),
          base::BindOnce(
              [](base::WeakPtr<TouchToFillDelegateAndroidImpl> delegate,
                 const std::u16string& value) {
                if (delegate) {
                  delegate->manager_->FillOrPreviewField(
                      mojom::ActionPersistence::kFill,
                      mojom::FieldActionType::kReplaceAll,
                      delegate->query_form_, delegate->query_field_, value,
                      SuggestionType::kIbanEntry, IBAN_VALUE);
                }
              },
              GetWeakPtr()));
}

void TouchToFillDelegateAndroidImpl::OnDismissed(bool dismissed_by_user) {
  if (IsShowingTouchToFill()) {
    ttf_payment_method_state_ = TouchToFillState::kWasShown;
    dismissed_by_user_ = dismissed_by_user;
  }
}

void TouchToFillDelegateAndroidImpl::LogMetricsAfterSubmission(
    const FormStructure& submitted_form) {
  // Log whether autofill was used after dismissing the touch to fill (without
  // selecting any credit card for filling)
  if (ttf_payment_method_state_ == TouchToFillState::kWasShown &&
      query_form_.global_id() == submitted_form.global_id() &&
      HasAnyAutofilledFields(submitted_form)) {
    base::UmaHistogramBoolean(
        "Autofill.TouchToFill.CreditCard.AutofillUsedAfterTouchToFillDismissal",
        dismissed_by_user_);
    if (!dismissed_by_user_) {
      base::UmaHistogramBoolean(
          "Autofill.TouchToFill.CreditCard.PerfectFilling",
          IsFillingPerfect(submitted_form));
      base::UmaHistogramBoolean(
          "Autofill.TouchToFill.CreditCard.FillingCorrectness",
          IsFillingCorrect(submitted_form));
    }
  }
}

bool TouchToFillDelegateAndroidImpl::HasAnyAutofilledFields(
    const FormStructure& submitted_form) const {
  return std::ranges::any_of(
      submitted_form, [](const auto& field) { return field->is_autofilled(); });
}

bool TouchToFillDelegateAndroidImpl::IsFillingPerfect(
    const FormStructure& submitted_form) const {
  return std::ranges::all_of(submitted_form, [](const auto& field) {
    return field->value(ValueSemantics::kCurrent).empty() ||
           field->is_autofilled();
  });
}

bool TouchToFillDelegateAndroidImpl::IsFillingCorrect(
    const FormStructure& submitted_form) const {
  return !std::ranges::any_of(submitted_form, [](const auto& field) {
    return field->previously_autofilled();
  });
}

bool TouchToFillDelegateAndroidImpl::IsFormPrefilled(const FormData& form) {
  return std::ranges::any_of(form.fields(), [&](const FormFieldData& field) {
    AutofillField* autofill_field = manager_->GetAutofillField(form, field);
    if (autofill_field && autofill_field->Type().GetStorableType() !=
                              FieldType::CREDIT_CARD_NUMBER) {
      return false;
    }
    return !SanitizedFieldIsEmpty(field.value());
  });
}

base::WeakPtr<TouchToFillDelegateAndroidImpl>
TouchToFillDelegateAndroidImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
