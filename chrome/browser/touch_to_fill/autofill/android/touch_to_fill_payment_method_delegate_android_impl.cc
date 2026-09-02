// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/touch_to_fill/autofill/android/touch_to_fill_payment_method_delegate_android_impl.h"

#include <optional>
#include <utility>
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
#include "components/autofill/core/browser/autofill_field.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/valuables/valuables_data_manager.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/valuables/loyalty_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/filling/filling_product.h"
#include "components/autofill/core/browser/form_structure.h"
#include "components/autofill/core/browser/form_types.h"
#include "components/autofill/core/browser/foundations/autofill_manager.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/logging/log_manager.h"
#include "components/autofill/core/browser/payments/bnpl_manager.h"
#include "components/autofill/core/browser/payments/iban_access_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator_util.h"
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
  return field.is_focusable() && SanitizedFieldIsEmpty(field.value());
}

// The form is considered correctly filled if all autofilled fields were not
// edited by user afterwards.
bool IsFillingCorrect(const FormStructure& form) {
  return std::ranges::none_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->last_modifier() != FieldModifier::kAutofill &&
               field->all_modifiers().contains(FieldModifier::kAutofill);
      });
}

// The form is considered perfectly filled if all non-empty fields are
// autofilled without further edits.
bool IsFillingPerfect(const FormStructure& form) {
  return std::ranges::all_of(
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->value().empty() ||
               field->last_modifier() == FieldModifier::kAutofill;
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
      form.fields(), [](const std::unique_ptr<AutofillField>& field) {
        return field->last_modifier() == FieldModifier::kAutofill;
      });
}

}  // namespace

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult::DryRunResult(
    TriggerOutcome outcome,
    std::variant<std::vector<CreditCard>,
                 std::vector<Iban>,
                 std::vector<LoyaltyCard>> items_to_suggest)
    : outcome(outcome), items_to_suggest(std::move(items_to_suggest)) {}

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult::DryRunResult(
    DryRunResult&&) = default;

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult&
TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult::operator=(
    DryRunResult&&) = default;

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult::~DryRunResult() =
    default;

TouchToFillPaymentMethodDelegateAndroidImpl::BnplCallbacks::BnplCallbacks() =
    default;
TouchToFillPaymentMethodDelegateAndroidImpl::BnplCallbacks::BnplCallbacks(
    BnplCallbacks&&) = default;
TouchToFillPaymentMethodDelegateAndroidImpl::BnplCallbacks&
TouchToFillPaymentMethodDelegateAndroidImpl::BnplCallbacks::operator=(
    BnplCallbacks&&) = default;
TouchToFillPaymentMethodDelegateAndroidImpl::BnplCallbacks::~BnplCallbacks() =
    default;

TouchToFillPaymentMethodDelegateAndroidImpl::
    TouchToFillPaymentMethodDelegateAndroidImpl(
        BrowserAutofillManager* manager)
    : manager_(CHECK_DEREF(manager)) {}

TouchToFillPaymentMethodDelegateAndroidImpl::
    ~TouchToFillPaymentMethodDelegateAndroidImpl() {
  // Invalidate pointers to avoid post hide callbacks.
  weak_ptr_factory_.InvalidateWeakPtrs();
  HideTouchToFill();
}

BrowserAutofillManager&
TouchToFillPaymentMethodDelegateAndroidImpl::GetAutofillManager() {
  return *manager_;
}

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult
TouchToFillPaymentMethodDelegateAndroidImpl::DryRun(FormGlobalId form_id,
                                                    FieldGlobalId field_id) {
  // Trigger only on supported platforms.
  if (!IsTouchToFillPaymentMethodSupported()) {
    return {TriggerOutcome::kUnsupportedFieldType, {}};
  }
  auto [form, field] = manager_->FindFormAndField(form_id, field_id);
  if (!form) {
    return {TriggerOutcome::kUnknownForm, {}};
  }
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
  if (!manager_->client().IsContextSecure()) {
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
    return DryRunForAffiliatedLoyaltyCard();
  }

  return {TriggerOutcome::kUnsupportedFieldType, {}};
}

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult
TouchToFillPaymentMethodDelegateAndroidImpl::DryRunForIban() {
  PersonalDataManager& pdm = manager_->client().GetPersonalDataManager();
  std::vector<Iban> ibans_to_suggest =
      pdm.payments_data_manager().GetOrderedIbansToSuggest();
  return ibans_to_suggest.empty()
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(ibans_to_suggest));
}

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult
TouchToFillPaymentMethodDelegateAndroidImpl::DryRunForCreditCard(
    const AutofillField& field,
    const FormStructure& form) {
  // Trigger only for complete forms (containing the fields for the card number
  // and the card expiration date).
  if (!FormHasAllCreditCardFields(form)) {
    return {TriggerOutcome::kIncompleteForm, {}};
  }
  if (IsFormPrefilled(form)) {
    return {TriggerOutcome::kFormAlreadyFilled, {}};
  }

  // Fetch all complete valid credit cards on file.
  // Complete = contains number, expiration date and name on card.
  // Valid = unexpired with valid number format.
  std::vector<CreditCard> cards_to_suggest = GetTouchToFillCardsToSuggest(
      manager_->client(), field, field.Type().GetCreditCardType());
  return cards_to_suggest.empty()
             ? DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {})
             : DryRunResult(TriggerOutcome::kShown,
                            std::move(cards_to_suggest));
}

TouchToFillPaymentMethodDelegateAndroidImpl::DryRunResult
TouchToFillPaymentMethodDelegateAndroidImpl::DryRunForAffiliatedLoyaltyCard() {
  ValuablesDataManager* vdm = manager_->client().GetValuablesDataManager();
  if (!vdm) {
    return DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {});
  }
  const std::vector<LoyaltyCard> loyalty_cards =
      vdm->GetLoyaltyCardsToSuggest();

  // Only show the TTF surface if any loyalty card have a matching merchant
  // domain.
  if (std::ranges::any_of(loyalty_cards, [&](const LoyaltyCard& loyalty_card) {
        return loyalty_card.GetAffiliationCategory(
                   manager_->client().GetLastCommittedPrimaryMainFrameURL()) ==
                   LoyaltyCard::AffiliationCategory::kAffiliated &&
               manager_->client().GetLastCommittedPrimaryMainFrameOrigin() ==
                   query_field_.origin();
      })) {
    return DryRunResult(TriggerOutcome::kShown, loyalty_cards);
  }
  return DryRunResult(TriggerOutcome::kNoValidPaymentMethods, {});
}

// TODO(crbug.com/40282650): Remove received FormData
bool TouchToFillPaymentMethodDelegateAndroidImpl::IntendsToShowTouchToFill(
    FormGlobalId form_id,
    FieldGlobalId field_id) {
  TriggerOutcome outcome = DryRun(form_id, field_id).outcome;
  LOG_AF(manager_->client().GetCurrentLogManager())
      << LoggingScope::kTouchToFill << LogMessage::kTouchToFill
      << "dry run before parsing for form " << form_id << " and field "
      << field_id << " was " << (outcome == TriggerOutcome::kShown ? "" : "un")
      << "successful (" << std::to_underlying(outcome) << ")";
  return outcome == TriggerOutcome::kShown;
}

bool TouchToFillPaymentMethodDelegateAndroidImpl::TryToShowTouchToFill(
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
        absl::Overload{
            [&](std::vector<CreditCard> items_to_suggest) {
              return payments_client.ShowTouchToFillCreditCard(
                  GetWeakPtr(), GetCreditCardSuggestionsForTouchToFill(
                                    std::move(items_to_suggest), *manager_,
                                    form.global_id()));
            },
            [&](std::vector<Iban> items_to_suggest) {
              return payments_client.ShowTouchToFillIban(
                  GetWeakPtr(), std::move(items_to_suggest));
            },
            [&](std::vector<LoyaltyCard> items_to_suggest) {
              return payments_client.ShowTouchToFillAffiliatedLoyaltyCard(
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
      << "successful (" << std::to_underlying(dry_run.outcome) << ")";

  if (dry_run.outcome != TriggerOutcome::kShown) {
    return false;
  }

  ttf_payment_method_state_ = TouchToFillState::kIsShowing;
  manager_->client().HideSuggestions(
      SuggestionHidingReason::kOverlappingWithTouchToFillSurface,
      /*product=*/std::nullopt);
  if (std::get_if<std::vector<CreditCard>>(&dry_run.items_to_suggest)) {
    manager_->DidShowSuggestions({Suggestion(SuggestionType::kCreditCardEntry)},
                                 /*metadata=*/{}, form.global_id(),
                                 field.global_id(),
                                 /*update_suggestions_callback=*/{});
  } else if (std::get_if<std::vector<LoyaltyCard>>(&dry_run.items_to_suggest)) {
    manager_->DidShowSuggestions(
        {Suggestion(SuggestionType::kLoyaltyCardEntry)},
        /*metadata=*/{}, form.global_id(), field.global_id(),
        /*update_suggestions_callback=*/{});
  } else {
    manager_->DidShowSuggestions({Suggestion(SuggestionType::kIbanEntry)},
                                 /*metadata=*/{}, form.global_id(),
                                 field.global_id(),
                                 /*update_suggestions_callback=*/{});
  }
  return true;
}

bool TouchToFillPaymentMethodDelegateAndroidImpl::
    ShowTouchToFillForAllLoyaltyCards(const FormData& form,
                                      const FormFieldData& field) {
  query_form_ = form;
  query_field_ = field;
  payments::PaymentsAutofillClient& payments_client =
      *manager_->client().GetPaymentsAutofillClient();
  ValuablesDataManager* vdm = manager_->client().GetValuablesDataManager();
  if (!vdm) {
    return false;
  }
  const std::vector<LoyaltyCard> loyalty_cards =
      vdm->GetLoyaltyCardsToSuggest();
  const bool shown = payments_client.ShowTouchToFillForAllLoyaltyCards(
      GetWeakPtr(), std::move(loyalty_cards));
  if (!shown) {
    return false;
  }
  ttf_payment_method_state_ = TouchToFillState::kIsShowing;
  manager_->client().HideSuggestions(
      SuggestionHidingReason::kOverlappingWithTouchToFillSurface,
      /*product=*/std::nullopt);
  manager_->DidShowSuggestions({Suggestion(SuggestionType::kLoyaltyCardEntry)},
                               /*metadata=*/{}, form.global_id(),
                               field.global_id(),
                               /*update_suggestions_callback=*/{});
  return true;
}

bool TouchToFillPaymentMethodDelegateAndroidImpl::IsShowingTouchToFill() {
  return ttf_payment_method_state_ == TouchToFillState::kIsShowing;
}

// TODO(crbug.com/40233391): Create a central point for TTF hiding decision.
void TouchToFillPaymentMethodDelegateAndroidImpl::HideTouchToFill() {
  if (IsShowingTouchToFill()) {
    manager_->client()
        .GetPaymentsAutofillClient()
        ->HideTouchToFillPaymentMethod();
  }
}

void TouchToFillPaymentMethodDelegateAndroidImpl::Reset() {
  HideTouchToFill();
  ttf_payment_method_state_ = TouchToFillState::kShouldShow;
}

bool TouchToFillPaymentMethodDelegateAndroidImpl::ShouldShowScanCreditCard() {
  return manager_->client()
             .GetPaymentsAutofillClient()
             ->HasCreditCardScanFeature() &&
         manager_->client().IsContextSecure();
}

bool TouchToFillPaymentMethodDelegateAndroidImpl::ShouldShowGPayLogo() const {
  return !manager_->client()
              .GetPaymentsAutofillClient()
              ->GetPaymentsDataManager()
              .HasAllLocalCreditCards();
}

void TouchToFillPaymentMethodDelegateAndroidImpl::ScanCreditCard() {
  manager_->client().GetPaymentsAutofillClient()->ScanCreditCard(
      base::BindOnce(&TouchToFillPaymentMethodDelegateAndroidImpl::
                         OnCreditCardScanned,
                     GetWeakPtr()));
}

void TouchToFillPaymentMethodDelegateAndroidImpl::OnCreditCardScanned(
    const CreditCard& card) {
  HideTouchToFill();
  manager_->FillOrPreviewForm(mojom::ActionPersistence::kFill,
                              query_form_.global_id(), query_field_.global_id(),
                              &card, AutofillTriggerSource::kScanCreditCard,
                              /*blocked_fields=*/{});
}

void TouchToFillPaymentMethodDelegateAndroidImpl::ShowPaymentMethodSettings() {
  manager_->client().ShowAutofillSettings(SuggestionType::kManageCreditCard);
}

void TouchToFillPaymentMethodDelegateAndroidImpl::
    CreditCardSuggestionSelected(std::string unique_id, bool is_virtual) {
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
  manager_->FillOrPreviewForm(
      mojom::ActionPersistence::kFill, query_form_.global_id(),
      query_field_.global_id(), &card_to_fill,
      AutofillTriggerSource::kKeyboardAccessoryOrBottomSheet,
      /*blocked_fields=*/{});
}

void TouchToFillPaymentMethodDelegateAndroidImpl::BnplSuggestionSelected(
    std::optional<int64_t> extracted_amount) {
  payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
  CHECK(bnpl_manager);
  bnpl_manager->OnUserDecisionToUseBnpl(
      extracted_amount,
      /*on_bnpl_vcn_fetched_callback=*/base::BindOnce(
          [](base::WeakPtr<TouchToFillPaymentMethodDelegateAndroidImpl>
                 delegate,
             const CreditCard& card) {
            if (delegate) {
              delegate->manager_->FillOrPreviewForm(
                  mojom::ActionPersistence::kFill,
                  delegate->query_form_.global_id(),
                  delegate->query_field_.global_id(), &card,
                  AutofillTriggerSource::kKeyboardAccessoryOrBottomSheet,
                  /*blocked_fields=*/{});
            }
          },
          GetWeakPtr()));
}

void TouchToFillPaymentMethodDelegateAndroidImpl::
    OnUserDecisionToUseSavedCards() {
  payments::BnplManager* bnpl_manager = manager_->GetPaymentsBnplManager();
  if (bnpl_manager) {
    bnpl_manager->OnUserDecisionToUseSavedCards();
  }
}

void TouchToFillPaymentMethodDelegateAndroidImpl::IbanSuggestionSelected(
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
              [](base::WeakPtr<TouchToFillPaymentMethodDelegateAndroidImpl>
                     delegate,
                 base::expected<std::u16string,
                                IbanAccessManager::FailureReason> result) {
                if (delegate && result.has_value()) {
                  delegate->manager_->FillOrPreviewField(
                      mojom::ActionPersistence::kFill,
                      mojom::FieldActionType::kReplaceAll,
                      delegate->query_form_.global_id(),
                      delegate->query_field_.global_id(), *result,
                      FillingProduct::kIban, IBAN_VALUE);
                }
              },
              GetWeakPtr()));
}

void TouchToFillPaymentMethodDelegateAndroidImpl::
    LoyaltyCardSuggestionSelected(const LoyaltyCard& loyalty_card) {
  HideTouchToFill();

  manager_->FillOrPreviewField(
      mojom::ActionPersistence::kFill, mojom::FieldActionType::kReplaceAll,
      query_form_.global_id(), query_field_.global_id(),
      base::UTF8ToUTF16(loyalty_card.loyalty_card_number()),
      FillingProduct::kLoyaltyCard, LOYALTY_MEMBERSHIP_ID);
  ValuablesDataManager* vdm = manager_->client().GetValuablesDataManager();
  CHECK(vdm);
  manager_->LogAndRecordLoyaltyCardFill(loyalty_card, query_form_.global_id(),
                                        query_field_.global_id());
}

void TouchToFillPaymentMethodDelegateAndroidImpl::OnDismissed(
    bool dismissed_by_user,
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

void TouchToFillPaymentMethodDelegateAndroidImpl::
    OnBnplIssuerSuggestionSelected(const std::string& issuer_id) {
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

void TouchToFillPaymentMethodDelegateAndroidImpl::OnBnplTosAccepted() {
  CHECK(bnpl_callbacks_.accept_tos_callback);
  std::move(bnpl_callbacks_.accept_tos_callback).Run();
}

void TouchToFillPaymentMethodDelegateAndroidImpl::LogTriggerOutcomeMetrics(
    const FormGlobalId& form_id,
    const FieldGlobalId& field_id,
    TriggerOutcome outcome) {
  if (outcome == TriggerOutcome::kUnsupportedFieldType) {
    return;
  }
  auto [form, field] = manager_->FindFormAndField(form_id, field_id);
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

void TouchToFillPaymentMethodDelegateAndroidImpl::LogMetricsAfterSubmission(
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

void TouchToFillPaymentMethodDelegateAndroidImpl::SetCancelCallback(
    base::OnceClosure cancel_callback) {
  bnpl_callbacks_.cancel_callback = std::move(cancel_callback);
}

void TouchToFillPaymentMethodDelegateAndroidImpl::SetSelectedIssuerCallback(
    base::OnceCallback<void(BnplIssuer)> selected_issuer_callback) {
  bnpl_callbacks_.selected_issuer_callback =
      std::move(selected_issuer_callback);
}

void TouchToFillPaymentMethodDelegateAndroidImpl::SetBnplTosAcceptCallback(
    base::OnceClosure accept_tos_callback) {
  bnpl_callbacks_.accept_tos_callback = std::move(accept_tos_callback);
}

base::WeakPtr<TouchToFillPaymentMethodDelegateAndroidImpl>
TouchToFillPaymentMethodDelegateAndroidImpl::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}

}  // namespace autofill
