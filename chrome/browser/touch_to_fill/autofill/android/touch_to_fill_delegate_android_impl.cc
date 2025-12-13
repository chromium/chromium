// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_delegate_android_impl.h"

#include <optional>
#include <variant>
#include <vector>

#include "base/check_deref.h"
#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/callback.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/utf_string_conversions.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/autofill/core/browser/autofill_browser_util.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/fast_checkout/fast_checkout_client.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"
#include "components/autofill/core/browser/suggestions/suggestion_hiding_reason.h"
#include "components/autofill/core/browser/suggestions/suggestion_type.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_internals/log_message.h"
#include "components/autofill/core/common/autofill_internals/logging_scope.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_util.h"
#include "components/autofill/core/common/logging/log_macros.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "third_party/abseil-cpp/absl/functional/overload.h"

namespace autofill {

namespace {

// Checks if the field is focusable and empty.
bool IsFieldFocusableAndEmpty(const AutofillField& field) {
  return field.IsFocusable() && SanitizedFieldIsEmpty(field.value());
}

// The form is considered correctly filled if all autofilled fields were not
// edited by user afterwards.
bool IsFillingCorrect(const FormStructure& form) {
  return !std::ranges::any_of(form.fields(), [](const auto& field) {
    return field->previously_autofilled();
  });
}

// The form is considered perfectly filled if all non-empty fields are
// autofilled without further edits.
bool IsFillingPerfect(const FormStructure& form) {
  return std::ranges::all_of(form.fields(), [](const auto& field) {
    return field->value().empty() || field->is_autofilled();
  });
}

// Checks if the credit card form is already filled with values. The form is
// considered to be filled if the credit card number field is non-empty. The
// expiration date fields are not checked because they might have arbitrary
// placeholders.
bool IsFormPrefilled(const FormStructure& form) {
  return std::ranges::any_of(form.fields(),
                             [](const std::unique_ptr<AutofillField>& field) {
                               return field->Type().GetCreditCardType() ==
                                          FieldType::CREDIT_CARD_NUMBER &&
                                      !SanitizedFieldIsEmpty(field->value());
                             });
}

bool HasAnyAutofilledFields(const FormStructure& form) {
  return std::ranges::any_of(
      form.fields(), [](const auto& field) { return field->is_autofilled(); });
}

}  // namespace

TouchToFillDelegateAndroidImpl::DryRunResult::DryRunResult(
    TriggerOutcome outcome,
    std::variant<std::vector<CreditCard>,
                 std::vector<Iban>,
                 std::vector<LoyaltyCard>> items_to_suggest)
    : outcome(outcome), items_to_suggest(std::move(items_to_suggest)) {}

TouchToFillDelegateAndroidImpl::DryRunResult::DryRunResult(DryRunResult&&) =
    default;

TouchToFillDelegateAndroidImpl::DryRunResult&
TouchToFillDelegateAndroidImpl::DryRunResult::operator=(DryRunResult&&) =
    default;

TouchToFillDelegateAndroidImpl::DryRunResult::~DryRunResult() = default;

TouchToFillDelegateAndroidImpl::BnplCallbacks::BnplCallbacks() = default;
TouchToFillDelegateAndroidImpl::BnplCallbacks::BnplCallbacks(BnplCallbacks&&) =
    default;
TouchToFillDelegateAndroidImpl::BnplCallbacks&
TouchToFillDelegateAndroidImpl::BnplCallbacks::operator=(BnplCallbacks&&) =
    default;
TouchToFillDelegateAndroidImpl::BnplCallbacks::~BnplCallbacks() = default;

TouchToFillDelegateAndroidImpl::TouchToFillDelegateAndroidImpl(
    BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillDelegateAndroidImpl::~TouchToFillDelegateAndroidImpl() {
  // Invalidate pointers to avoid post hide callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  HideTouchToFill();
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRun(FormGlobalId form_id,
                                       FieldGlobalId field_id) {
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
  // Trigger only if Touch To Fill should not be shown or reshown.
  if (ttf_payment_method_state_ != TouchToFillState::kShouldShow &&
      ttf_payment_method_state_ !=
          TouchToFillState::kShownAndShouldBeShownAgain) {
    return {TriggerOutcome::kShownBeforeAndShouldNotBeShownAgain, {}};
  }

  // Trigger only if the client and the form are not insecure.
  if (IsFormOrClientNonSecure(manager_->client(), *form)) {
    return {TriggerOutcome::kFormOrClientNotSecure, {}};
  }
  // Trigger only on focusable empty field.
  if (!IsFieldFocusableAndEmpty(*field)) {
    return {TriggerOutcome::kFieldNotEmptyOrNotFocusable, {}};
  }
  // Trigger only if the UI is available.
  if (!manager_->CanShowAutofillUi()) {
    return {TriggerOutcome::kCannotShowAutofillUi, {}};
  }

  if (field->Type().GetGroups().contains(FieldTypeGroup::kIban)) {
    return DryRunForIban();
  } else if (field->Type().GetGroups().contains(FieldTypeGroup::kCreditCard)) {
    return DryRunForCreditCard(*field, *form);
  } else if (field->Type().GetGroups().contains(FieldTypeGroup::kLoyaltyCard) ||
             field->Type().GetLoyaltyCardType() ==
                 EMAIL_OR_LOYALTY_MEMBERSHIP_ID) {
    return DryRunForLoyaltyCard();
  }

  return {TriggerOutcome::kUnsupportedFieldType, {}};
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRunForIban() {
  PersonalDataManager& pdm = manager_->client().GetPersonalDataManager();
  std::vector<Iban> ibans_to_suggest =
      pdm.payments_data_manager().GetOrderedIbansToSuggest();
  return ibans_to_suggest.empty()
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(ibans_to_suggest));
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRunForCreditCard(const AutofillField& field,
                                                    const FormStructure& form) {
  // Trigger only for complete forms (containing the fields for the card number
  // and the card expiration date).
  if (!FormHasAllCreditCardFields(form)) {
    return {TriggerOutcome::kIncompleteForm, {}};
  }
  if (IsFormPrefilled(form)) {
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
      manager_->client(), field, field.Type().GetCreditCardType());
  return cards_to_suggest.empty()
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(cards_to_suggest));
}

TouchToFillDelegateAndroidImpl::DryRunResult
TouchToFillDelegateAndroidImpl::DryRunForLoyaltyCard() {
  ValuablesDataManager* vdm = manager_->client().GetValuablesDataManager();
  if (!vdm) {
    return DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {});
  }
  const std::vector<LoyaltyCard> loyalty_cards =
      vdm->GetLoyaltyCardsToSuggest();

  // Only show the TTF surface if any loyalty card have a matching merchant
  // domain.
  const GURL& current_domain =
      manager_->client().GetLastCommittedPrimaryMainFrameURL();
  if (std::ranges::any_of(
          loyalty_cards, [&current_domain](const LoyaltyCard& loyalty_card) {
            return loyalty_card.GetAffiliationCategory(current_domain) ==
                   LoyaltyCard::AffiliationCategory::kAffiliated;
          })) {
    return DryRunResult(TriggerOutcome::kShown, loyalty_cards);
  }
  return DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {});
}

// TODO(crbug.com/40282650): Remove received FormData
bool TouchToFillDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  TriggerOutcome outcome = DryRun(form_id, field_id).outcome;
  LOG_AF(manager_->client().GetCurrentLogManager())
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
  DryRunResult dry_run = DryRun(form.global_id(), field.global_id());
  if (dry_run.outcome == TriggerOutcome::kShown) {
    payments::PaymentsAutofillClient& payments_client =
        *manager_->client().GetPaymentsAutofillClient();
    const bool shown = std::visit(
        absl::Overload{[&](std::vector<CreditCard> items_to_suggest) {
                         return payments_client.ShowTouchToFillCreditCard(
                             GetWeakPtr(),
                             GetCreditCardSuggestionsForTouchToFill(
                                 std::move(items_to_suggest), *manager_));
                       },
                       [&](std::vector<Iban> items_to_suggest) {
                         return payments_client.ShowTouchToFillIban(
                             GetWeakPtr(), std::move(items_to_suggest));
                       },
                       [&](std::vector<LoyaltyCard> items_to_suggest) {
                         return payments_client.ShowTouchToFillLoyaltyCard(
                             GetWeakPtr(), std::move(items_to_suggest));
                       }},
        std::move(dry_run.items_to_suggest));
    if (!shown) {
      dry_run.outcome = TriggerOutcome::kFailedToDisplayBottomSheet;
    }
  }

  LogTriggerOutcomeMetrics(form.global_id(), field.global_id(),
                           dry_run.outcome);
  LOG_AF(manager_->client().GetCurrentLogManager())
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
  if (std::get_if<std::vector<CreditCard>>(&dry_run.items_to_suggest)) {
    manager_->DidShowSuggestions({Suggestion(SuggestionType::kCreditCardEntry)},
                                 form, field.global_id(),
                                 /*update_suggestions_callback=*/{});
  } else if (std::get_if<std::vector<LoyaltyCard>>(&dry_run.items_to_suggest)) {
    manager_->DidShowSuggestions(
        {Suggestion(SuggestionType::kLoyaltyCardEntry)}, form,
        field.global_id(),
        /*update_suggestions_callback=*/{});
  } else {
    manager_->DidShowSuggestions({Suggestion(SuggestionType::kIbanEntry)}, form,
                                 field.global_id(),
                                 /*update_suggestions_callback=*/{});
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
  manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill, query_form_,
                              query_field_.global_id(), &card,
                              AutofillTriggerSource::kScanCreditCard);
}

void TouchToFillDelegateAndroidImpl::ShowPaymentMethodSettings() {
  manager_->client().ShowAutofillSettings(SuggestionType::kManageCreditCard);
}

void TouchToFillDelegateAndroidImpl::CreditCardSuggestionSelected(
    std::string unique_id,
    bool is_virtual) {
  HideTouchToFill();

  PersonalDataManager& pdm = manager_->client().GetPersonalDataManager();
  const CreditCard* card =
      pdm.payments_data_manager().GetCreditCardByGUID(unique_id);
  // TODO(crbug.com/40071928): Figure out why `card` is sometimes nullptr.
  if (!card) {
    return;
  }
  const CreditCard& card_to_fill =
      is_virtual ? CreditCard::CreateVirtualCard(*card) : *card;
  manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill, query_form_,
                              query_field_.global_id(), &card_to_fill,
                              AutofillTriggerSource::kTouchToFillCreditCard);
}

void TouchToFillDelegateAndroidImpl::BnplSuggestionSelected(
    std::optional<int64_t> extracted_amount) {
  payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
  CHECK(bnpl_manager);
  bnpl_manager->OnDidAcceptBnplSuggestion(
      extracted_amount,
      /*on_bnpl_vcn_fetched_callback=*/base::BindOnce(
          [](base::WeakPtr<TouchToFillDelegateAndroidImpl> delegate,
             const CreditCard& card) {
            if (delegate) {
              delegate->manager_->FillOrPreviewForm(
                  mojom::ActionPersistence::kFill, delegate->query_form_,
                  delegate->query_field_.global_id(), &card,
                  AutofillTriggerSource::kTouchToFillCreditCard);
            }
          },
          GetWeakPtr()));
}

void TouchToFillDelegateAndroidImpl::IbanSuggestionSelected(
    std::variant<Iban::Guid, Iban::InstrumentId> backend_id) {
  HideTouchToFill();

  manager_->client()
      .GetPaymentsAutofillClient()
      ->GetIbanAccessManager()
      ->FetchValue(
          std::holds_alternative<Iban::Guid>(backend_id)
              ? Suggestion::Payload(
                    Suggestion::Guid(std::get<Iban::Guid>(backend_id).value()))
              : Suggestion::Payload(Suggestion::InstrumentId(
                    std::get<Iban::InstrumentId>(backend_id).value())),
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

void TouchToFillDelegateAndroidImpl::LoyaltyCardSuggestionSelected(
    const LoyaltyCard& loyalty_card) {
  HideTouchToFill();

  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_, query_field_,
      base::UTF8ToUTF16(loyalty_card.loyalty_card_number()),
      SuggestionType::kLoyaltyCardEntry, LOYALTY_MEMBERSHIP_ID);
  ValuablesDataManager* vdm = manager_->client().GetValuablesDataManager();
  CHECK(vdm);
  manager_->LogAndRecordLoyaltyCardFill(loyalty_card, query_form_.global_id(),
                                        query_field_.global_id());
}

void TouchToFillDelegateAndroidImpl::OnDismissed(bool dismissed_by_user,
                                                 bool should_reshow) {
  if (dismissed_by_user && bnpl_callbacks_.cancel_callback) {
    std::move(bnpl_callbacks_.cancel_callback).Run();
  } else {
    bnpl_callbacks_.cancel_callback.Reset();
  }

  if (IsShowingTouchToFill()) {
    ttf_payment_method_state_ =
        should_reshow && base::FeatureList::IsEnabled(
                             features::kAutofillEnableTouchToFillReshowForBnpl)
            ? TouchToFillState::kShownAndShouldBeShownAgain
            : TouchToFillState::kShownAndShouldNotBeShownAgain;
    dismissed_by_user_ = dismissed_by_user;
  }
}

void TouchToFillDelegateAndroidImpl::OnBnplIssuerSuggestionSelected(
    const std::string& issuer_id) {
  // This check is a safeguard. `selected_issuer_callback` is set in
  // `TouchToFillPaymentMethodControllerImpl::ShowBnplIssuers()` and should
  // always be non-null here.
  if (!bnpl_callbacks_.selected_issuer_callback) {
    return;
  }

  std::vector<BnplIssuer> issuers = manager_->client()
                                        .GetPaymentsAutofillClient()
                                        ->GetPaymentsDataManager()
                                        .GetBnplIssuers();
  for (BnplIssuer& issuer : issuers) {
    if (ConvertToBnplIssuerIdString(issuer.issuer_id()) == issuer_id) {
      std::move(bnpl_callbacks_.selected_issuer_callback)
          .Run(std::move(issuer));
      break;
    }
  }
}

void TouchToFillDelegateAndroidImpl::OnBnplTosAccepted() {
  CHECK(bnpl_callbacks_.accept_tos_callback);
  std::move(bnpl_callbacks_.accept_tos_callback).Run();
}

void TouchToFillDelegateAndroidImpl::LogTriggerOutcomeMetrics(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    TriggerOutcome outcome) {
  if (outcome == TriggerOutcome::kUnsupportedFieldType) {
    return;
  }
  const FormStructure* form = manager_->FindCachedFormById(form_id);
  const AutofillField* field = form ? form->GetFieldById(field_id) : nullptr;
  const FieldTypeGroupSet groups =
      field ? field->Type().GetGroups() : FieldTypeGroupSet{};
  if (groups.contains(FieldTypeGroup::kIban)) {
    base::UmaHistogramEnumeration(kUmaTouchToFillIbanTriggerOutcome, outcome);
  } else if (groups.contains(FieldTypeGroup::kLoyaltyCard)) {
    base::UmaHistogramEnumeration(kUmaTouchToFillLoyaltyCardTriggerOutcome,
                                  outcome);
  } else {
    base::UmaHistogramEnumeration(kUmaTouchToFillCreditCardTriggerOutcome,
                                  outcome);
  }
}

void TouchToFillDelegateAndroidImpl::LogMetricsAfterSubmission(
    const FormStructure& submitted_form) {
  // Log whether autofill was used after dismissing the touch to fill (without
  // selecting any credit card for filling).
  if ((ttf_payment_method_state_ ==
           TouchToFillState::kShownAndShouldNotBeShownAgain ||
       ttf_payment_method_state_ ==
           TouchToFillState::kShownAndShouldBeShownAgain) &&
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

void TouchToFillDelegateAndroidImpl::SetCancelCallback(
    base::OnceClosure cancel_callback) {
  bnpl_callbacks_.cancel_callback = std::move(cancel_callback);
}

void TouchToFillDelegateAndroidImpl::SetSelectedIssuerCallback(
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback) {
  bnpl_callbacks_.selected_issuer_callback =
      std::move(selected_issuer_callback);
}

void TouchToFillDelegateAndroidImpl::SetBnplTosAcceptCallback(
    base::OnceClosure accept_tos_callback) {
  bnpl_callbacks_.accept_tos_callback = std::move(accept_tos_callback);
}

base::WeakPtr<TouchToFillDelegateAndroidImpl>
TouchToFillDelegateAndroidImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
