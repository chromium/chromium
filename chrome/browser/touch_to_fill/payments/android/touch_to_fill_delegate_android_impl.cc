// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/payments/android/touch_to_fill_delegate_android_impl.h"

#include "base/feature_list.h"
#include "base/metrics/histogram_functions.h"
#include "chrome/browser/fast_checkout/fast_checkout_features.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/autofill_suggestion_generator.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_client.h"
#include "components/autofill/core/browser/ui/popup_types.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_macros.h"

namespace autofill {

TouchToFillDelegateAndroidImpl::DryRunResult::DryRunResult(
    TriggerOutcome outcome,
    std::vector<CreditCard> cards_to_suggest)
    : outcome(outcome), cards_to_suggest(std::move(cards_to_suggest)) {}

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

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRun(FormGlobalId form_id,
                                       FieldGlobalId field_id,
                                       const FormData* optional_received_form) {
  // Trigger only on supported platforms.
  if (!manager_->client().IsTouchToFillCreditCardSupported()) {
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
  // Trigger only for a credit card field/form.
  if (field->Type().group() != FieldTypeGroup::kCreditCard) {
    return {TriggerOutcome::kUnsupportedFieldType, {}};
  }

  // Trigger only for complete forms (containing the fields for the card number
  // and the card expiration date).
  if (!FormHasAllCreditCardFields(*form)) {
    return {TriggerOutcome::kIncompleteForm, {}};
  }
  if (optional_received_form != nullptr &&
      IsFormPrefilled(*optional_received_form)) {
    return {TriggerOutcome::kFormAlreadyFilled, {}};
  }
  // Trigger only if not shown before.
  if (ttf_credit_card_state_ != TouchToFillState::kShouldShow) {
    return {TriggerOutcome::kShownBefore, {}};
  }
  // Trigger only if the client and the form are not insecure.
  if (IsFormOrClientNonSecure(manager_->client(), *form)) {
    return {TriggerOutcome::kFormOrClientNotSecure, {}};
  }
  // Trigger only on focusable empty field.
  // TODO(crbug.com/1331312): This should be the field's *current* value, not
  // the original value.
  if (!field->IsFocusable() || !SanitizedFieldIsEmpty(field->value)) {
    return {TriggerOutcome::kFieldNotEmptyOrNotFocusable, {}};
  }
  // Trigger only if Fast Checkout was not shown before.
  if (base::FeatureList::IsEnabled(::features::kFastCheckout) &&
      !manager_->client().GetFastCheckoutClient()->IsNotShownYet()) {
    return {TriggerOutcome::kFastCheckoutWasShown, {}};
  }
  // Trigger only if there is at least 1 complete valid credit card on file.
  // Complete = contains number, expiration date and name on card.
  // Valid = unexpired with valid number format.
  std::vector<CreditCard> cards_to_suggest =
      AutofillSuggestionGenerator::GetOrderedCardsToSuggest(
          &manager_->client(), /*suppress_disused_cards=*/true);
  if (base::ranges::none_of(cards_to_suggest,
                            &CreditCard::IsCompleteValidCard)) {
    return {TriggerOutcome::kNoValidCards, {}};
  }
  // Trigger only if the UI is available.
  if (!manager_->CanShowAutofillUi()) {
    return {TriggerOutcome::kCannotShowAutofillUi, {}};
  }

  // If the card is enrolled into virtual card number, create a copy of the
  // card with `CreditCard::RecordType::kVirtualCard` as the record type, and
  // insert it before the actual card.
  std::vector<autofill::CreditCard> real_and_virtual_cards;
  for (const CreditCard& card : cards_to_suggest) {
    if (card.virtual_card_enrollment_state() ==
            CreditCard::VirtualCardEnrollmentState::kEnrolled &&
        base::FeatureList::IsEnabled(
            features::kAutofillVirtualCardsOnTouchToFillAndroid)) {
      real_and_virtual_cards.push_back(CreditCard::CreateVirtualCard(card));
    }
    real_and_virtual_cards.push_back(card);
  }
  return {TriggerOutcome::kShown, std::move(real_and_virtual_cards)};
}

bool TouchToFillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  // optional_received_form is not available to pass here.
  TriggerOutcome outcome = DryRun(form_id, field_id).outcome;
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
  // TODO(crbug.com/1386143): store only FormGlobalId and FieldGlobalId instead
  // to avoid that FormData and FormFieldData may become obsolete during the
  // bottomsheet being open.
  query_form_ = form;
  query_field_ = field;
  DryRunResult dry_run = DryRun(form.global_id(), field.global_id(), &form);
  if (dry_run.outcome == TriggerOutcome::kShown &&
      !manager_->client().ShowTouchToFillCreditCard(
          GetWeakPtr(), std::move(dry_run.cards_to_suggest))) {
    dry_run.outcome = TriggerOutcome::kFailedToDisplayBottomSheet;
  }
  if (dry_run.outcome != TriggerOutcome::kUnsupportedFieldType) {
    base::UmaHistogramEnumeration(kUmaTouchToFillCreditCardTriggerOutcome,
                                  dry_run.outcome);
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

  ttf_credit_card_state_ = TouchToFillState::kIsShowing;
  manager_->client().HideAutofillPopup(
      PopupHidingReason::kOverlappingWithTouchToFillSurface);
  manager_->DidShowSuggestions(/*has_autofill_suggestions=*/true, form, field);
  return true;
}

bool TouchToFillDelegateAndroidImpl::IsShowingTouchToFill() {
  return ttf_credit_card_state_ == TouchToFillState::kIsShowing;
}

// TODO(crbug.com/1348538): Create a central point for TTF hiding decision.
void TouchToFillDelegateAndroidImpl::HideTouchToFill() {
  if (IsShowingTouchToFill()) {
    // TODO(crbug.com/1417442): This is to prevent calling virtual functions in
    // destructors in the following call chain:
    //       ~ContentAutofillDriver()
    //   --> ~BrowserAutofillManager()
    //   --> ~TouchToFillDelegateAndroidImpl()
    //   --> HideTouchToFill()
    //   --> AutofillManager::safe_client()
    //   --> ContentAutofillDriver::IsPrerendering()
    manager_->unsafe_client(/*pass_key=*/{}).HideTouchToFillCreditCard();
  }
}

void TouchToFillDelegateAndroidImpl::Reset() {
  HideTouchToFill();
  ttf_credit_card_state_ = TouchToFillState::kShouldShow;
}

AutofillManager* TouchToFillDelegateAndroidImpl::GetManager() {
  return manager_;
}

bool TouchToFillDelegateAndroidImpl::ShouldShowScanCreditCard() {
  if (!manager_->client().HasCreditCardScanFeature()) {
    return false;
  }

  return !IsFormOrClientNonSecure(manager_->client(), query_form_);
}

void TouchToFillDelegateAndroidImpl::ScanCreditCard() {
  manager_->client().ScanCreditCard(base::BindOnce(
      &TouchToFillDelegateAndroidImpl::OnCreditCardScanned, GetWeakPtr()));
}

void TouchToFillDelegateAndroidImpl::OnCreditCardScanned(
    const CreditCard& card) {
  HideTouchToFill();
  manager_->FillCreditCardFormImpl(
      query_form_, query_field_, card, std::u16string(),
      {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
}

void TouchToFillDelegateAndroidImpl::ShowCreditCardSettings() {
  manager_->client().ShowAutofillSettings(PopupType::kCreditCards);
}

void TouchToFillDelegateAndroidImpl::SuggestionSelected(std::string unique_id,
                                                        bool is_virtual) {
  HideTouchToFill();

  if (is_virtual) {
    manager_->FillOrPreviewVirtualCardInformation(
        mojom::AutofillActionPersistence::kFill, unique_id, query_form_,
        query_field_,
        {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
  } else {
    PersonalDataManager* pdm = manager_->client().GetPersonalDataManager();
    DCHECK(pdm);
    CreditCard* card = pdm->GetCreditCardByGUID(unique_id);
    manager_->FillOrPreviewCreditCardForm(
        mojom::AutofillActionPersistence::kFill, query_form_, query_field_,
        card,
        {.trigger_source = AutofillTriggerSource::kTouchToFillCreditCard});
  }
}

void TouchToFillDelegateAndroidImpl::OnDismissed(bool dismissed_by_user) {
  if (IsShowingTouchToFill()) {
    ttf_credit_card_state_ = TouchToFillState::kWasShown;
    dismissed_by_user_ = dismissed_by_user;
  }
}

void TouchToFillDelegateAndroidImpl::LogMetricsAfterSubmission(
    const FormStructure& submitted_form) {
  // Log whether autofill was used after dismissing the touch to fill (without
  // selecting any credit card for filling)
  if (ttf_credit_card_state_ == TouchToFillState::kWasShown &&
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
  return base::ranges::any_of(
      submitted_form, [](const auto& field) { return field->is_autofilled; });
}

bool TouchToFillDelegateAndroidImpl::IsFillingPerfect(
    const FormStructure& submitted_form) const {
  return base::ranges::all_of(submitted_form, [](const auto& field) {
    return field->value.empty() || field->is_autofilled;
  });
}

bool TouchToFillDelegateAndroidImpl::IsFillingCorrect(
    const FormStructure& submitted_form) const {
  return !base::ranges::any_of(submitted_form, [](const auto& field) {
    return field->previously_autofilled();
  });
}

bool TouchToFillDelegateAndroidImpl::IsFormPrefilled(const FormData& form) {
  return base::ranges::any_of(form.fields, [&](const FormFieldData& field) {
    AutofillField* autofill_field = manager_->GetAutofillField(form, field);
    if (autofill_field->Type().GetStorableType() !=
        ServerFieldType::CREDIT_CARD_NUMBER) {
      return false;
    }
    return !SanitizedFieldIsEmpty(field.value);
  });
}

base::WeakPtr<TouchToFillDelegateAndroidImpl>
TouchToFillDelegateAndroidImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
