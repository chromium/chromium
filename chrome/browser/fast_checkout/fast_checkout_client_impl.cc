// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_client_impl.h"

#include <algorithm>
#include <cmath>

#include "base/containers/flat_set.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/metrics_hashes.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "chrome/browser/fast_checkout/fast_checkout_accessibility_service_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_capabilities_fetcher_factory.h"
#include "chrome/browser/fast_checkout/fast_checkout_delegate_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"
#include "chrome/browser/fast_checkout/fast_checkout_trigger_validator_impl.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/metrics/autofill_metrics_utils.h"
#include "components/autofill/core/browser/payments/credit_card_cvc_authenticator.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/ui/fast_checkout_enums.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/autofill/core/common/mojom/autofill_types.mojom-shared.h"
#include "components/autofill/core/common/signatures.h"
#include "content/public/browser/web_contents_user_data.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

namespace {
using ::autofill::FastCheckoutRunOutcome;
using ::autofill::FastCheckoutTriggerOutcome;
using ::autofill::FastCheckoutUIState;

constexpr base::TimeDelta kSleepBetweenTriggerFormExtractionCalls =
    base::Seconds(1);
constexpr base::TimeDelta kTimeout = base::Minutes(30);

constexpr auto kSupportedFormTypes = base::MakeFixedFlatSet<autofill::FormType>(
    {autofill::FormType::kAddressForm, autofill::FormType::kCreditCardForm});

constexpr auto kAddressFieldTypes =
    base::MakeFixedFlatSet<autofill::FieldTypeGroup>(
        {autofill::FieldTypeGroup::kName, autofill::FieldTypeGroup::kEmail,
         autofill::FieldTypeGroup::kPhone, autofill::FieldTypeGroup::kAddress});

bool IsVisibleTextField(const autofill::AutofillField& field) {
  return field.IsFocusable() && field.IsTextInputElement();
}

autofill::AutofillField* GetFieldToFill(
    const std::vector<std::unique_ptr<autofill::AutofillField>>& fields,
    bool is_credit_card_form) {
  for (const std::unique_ptr<autofill::AutofillField>& field : fields) {
    if (IsVisibleTextField(*field) &&
        field->value(autofill::ValueSemantics::kCurrent).empty() &&
        ((!is_credit_card_form &&
          kAddressFieldTypes.contains(field->Type().group())) ||
         (is_credit_card_form &&
          field->Type().GetStorableType() == autofill::CREDIT_CARD_NUMBER))) {
      return field.get();
    }
  }
  return nullptr;
}

bool IsNameOrAddress(autofill::FieldTypeGroup type_group) {
  return type_group == autofill::FieldTypeGroup::kName ||
         type_group == autofill::FieldTypeGroup::kAddress;
}

// Returns `true` if `form` is considered an address form containing only an
// `email` field but no `name` or `address` fields.
bool IsEmailForm(const autofill::FormStructure& form) {
  // `kAddressForm` includes email fields.
  bool is_address_form =
      form.GetFormTypes().contains(autofill::FormType::kAddressForm);
  bool has_name_or_address_field = std::ranges::any_of(
      form.fields(), [](const std::unique_ptr<autofill::AutofillField>& field) {
        autofill::FieldTypeGroup type_group = field->Type().group();
        return IsNameOrAddress(type_group) && IsVisibleTextField(*field);
      });
  bool has_focusable_email_field = std::ranges::any_of(
      form.fields(), [](const std::unique_ptr<autofill::AutofillField>& field) {
        return field->Type().group() == autofill::FieldTypeGroup::kEmail &&
               IsVisibleTextField(*field);
      });
  return is_address_form && has_focusable_email_field &&
         !has_name_or_address_field;
}

// Returns `true` if `form_signature`'s form is in `forms` and is an email form.
bool ContainsEmailFormWithSignature(
    const std::map<autofill::FormGlobalId,
                   std::unique_ptr<autofill::FormStructure>>& forms,
    autofill::FormSignature form_signature) {
  for (auto& [_, form] : forms) {
    // It is possible to have multiple forms with the same form signature on the
    // same page where only some are visible to the user. An example could be
    // shipping and billing address forms. For that reason the `IsEmailForm`
    // check must not be returned directly to avoid a premature return as we
    // don't have any control over the order of `forms`.
    if (form->form_signature() == form_signature && IsEmailForm(*form)) {
      return true;
    }
  }
  return false;
}

FastCheckoutDelegateImpl* GetDelegate(autofill::AutofillManager& manager) {
  auto& bam = static_cast<autofill::BrowserAutofillManager&>(manager);
  return static_cast<FastCheckoutDelegateImpl*>(bam.fast_checkout_delegate());
}
}  // namespace

// No virtual functions of `client` must be called in the constructor.
FastCheckoutClientImpl::FastCheckoutClientImpl(
    autofill::ContentAutofillClient* client)
    : autofill_client_(client),
      fetcher_(FastCheckoutCapabilitiesFetcherFactory::GetForBrowserContext(
          client->GetWebContents().GetBrowserContext())),
      personal_data_helper_(
          std::make_unique<FastCheckoutPersonalDataHelperImpl>(
              &client->GetWebContents())),
      trigger_validator_(std::make_unique<FastCheckoutTriggerValidatorImpl>(
          autofill_client_,
          fetcher_,
          personal_data_helper_.get())),
      accessibility_service_(
          std::make_unique<FastCheckoutAccessibilityServiceImpl>()),
      keyboard_suppressor_(
          client,
          base::BindRepeating([](autofill::AutofillManager& manager) {
            return GetDelegate(manager) &&
                   GetDelegate(manager)->IsShowingFastCheckoutUI();
          }),
          base::BindRepeating([](autofill::AutofillManager& manager,
                                 autofill::FormGlobalId form,
                                 autofill::FieldGlobalId field,
                                 const autofill::FormData& form_data) {
            return GetDelegate(manager) &&
                   GetDelegate(manager)->IntendsToShowFastCheckout(
                       manager, form, field, form_data);
          }),
          base::Seconds(1)) {
  driver_factory_observation_.Observe(
      &autofill_client_->GetAutofillDriverFactory());
}

FastCheckoutClientImpl::~FastCheckoutClientImpl() = default;

void FastCheckoutClientImpl::OnContentAutofillDriverFactoryDestroyed(
    autofill::ContentAutofillDriverFactory& factory) {
  driver_factory_observation_.Reset();
}

void FastCheckoutClientImpl::OnContentAutofillDriverCreated(
    autofill::ContentAutofillDriverFactory& factory,
    autofill::ContentAutofillDriver& driver) {
  auto& manager = static_cast<autofill::BrowserAutofillManager&>(
      driver.GetAutofillManager());
  manager.set_fast_checkout_delegate(std::make_unique<FastCheckoutDelegateImpl>(
      &autofill_client_->GetWebContents(), this, &manager));
}

bool FastCheckoutClientImpl::TryToStart(
    const GURL& url,
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    base::WeakPtr<autofill::AutofillManager> autofill_manager) {
  if (!keyboard_suppressor_.is_suppressing()) {
    return false;
  }

  if (!autofill_manager) {
    return false;
  }

  FastCheckoutTriggerOutcome trigger_outcome = trigger_validator_->ShouldRun(
      form, field, fast_checkout_ui_state_, is_running_, *autofill_manager);

  if (trigger_outcome != FastCheckoutTriggerOutcome::kSuccess) {
    return false;
  }

  autofill_manager_ = autofill_manager;
  origin_ = url::Origin::Create(url);
  is_running_ = true;
  personal_data_manager_observation_.Observe(
      personal_data_helper_->GetPersonalDataManager());
  autofill_manager_observation_.Observe(autofill_manager_.get());
  run_id_ =
      base::HashMetricName(base::Uuid::GenerateRandomV4().AsLowercaseString());

  SetFormsToFill();

  fast_checkout_controller_ = CreateFastCheckoutController();
  ShowFastCheckoutUI();

  fast_checkout_ui_state_ = FastCheckoutUIState::kIsShowing;
  autofill_client_->HideAutofillSuggestions(
      autofill::SuggestionHidingReason::kOverlappingWithFastCheckoutSurface);

  return true;
}

void FastCheckoutClientImpl::ShowFastCheckoutUI() {
  fast_checkout_controller_->Show(
      personal_data_helper_->GetProfilesToSuggest(),
      personal_data_helper_->GetCreditCardsToSuggest());
}

void FastCheckoutClientImpl::OnRunComplete(FastCheckoutRunOutcome run_outcome,
                                           bool allow_further_runs) {
  ukm::builders::FastCheckout_RunOutcome run_outcome_builder(
      autofill_client_->GetWebContents()
          .GetPrimaryMainFrame()
          ->GetPageUkmSourceId());
  run_outcome_builder.SetRunOutcome(static_cast<int64_t>(run_outcome));
  run_outcome_builder.SetRunId(run_id_);
  run_outcome_builder.Record(ukm::UkmRecorder::Get());

  if (autofill_manager_) {
    for (auto [form_id, filling_state] : form_filling_states_) {
      autofill::FormSignature form_signature = form_id.first;
      autofill::DenseSet<autofill::FormTypeNameForLogging> form_types;
      for (auto& [_, form] : autofill_manager_->form_structures()) {
        if (form->form_signature() == form_signature) {
          form_types =
              autofill::autofill_metrics::GetFormTypesForLogging(*form);
          break;
        }
      }
      ukm::builders::FastCheckout_FormStatus form_status_builder(
          autofill_client_->GetWebContents()
              .GetPrimaryMainFrame()
              ->GetPageUkmSourceId());
      form_status_builder.SetFilled(filling_state == FillingState::kFilled);
      form_status_builder.SetFormSignature(
          autofill::HashFormSignature(form_signature));
      form_status_builder.SetRunId(run_id_);
      form_status_builder.SetFormTypes(
          autofill::AutofillMetrics::FormTypesToBitVector(form_types));
      form_status_builder.Record(ukm::UkmRecorder::Get());
    }
  }

  InternalStop(allow_further_runs);
}

void FastCheckoutClientImpl::InternalStop(bool allow_further_runs) {
  // `OnHidden` is not called if the bottom sheet never managed to show,
  // e.g. due to a failed onboarding. This ensures that keyboard suppression
  // stops.
  keyboard_suppressor_.Unsuppress();

  // Reset run related state.
  is_running_ = false;
  form_filling_states_.clear();
  form_signatures_to_fill_.clear();
  selected_autofill_profile_guid_ = std::nullopt;
  selected_credit_card_id_ = std::nullopt;
  timeout_timer_.AbandonAndStop();
  credit_card_form_global_id_ = std::nullopt;
  run_id_ = 0;
  // Reset UI related state.
  fast_checkout_controller_.reset();
  // Reset personal data manager observation.
  personal_data_manager_observation_.Reset();
  // Reset `autofill_manager_` and related objects.
  form_extraction_timer_.AbandonAndStop();
  autofill_manager_observation_.Reset();
  autofill_manager_.reset();

  if (!allow_further_runs) {
    fast_checkout_ui_state_ = FastCheckoutUIState::kWasShown;
  } else {
    fast_checkout_ui_state_ = FastCheckoutUIState::kNotShownYet;
  }
}

void FastCheckoutClientImpl::Stop(bool allow_further_runs) {
  InternalStop(allow_further_runs || !IsShowing());
}

bool FastCheckoutClientImpl::IsShowing() const {
  return fast_checkout_ui_state_ == FastCheckoutUIState::kIsShowing;
}

bool FastCheckoutClientImpl::IsRunning() const {
  return is_running_;
}

std::unique_ptr<FastCheckoutController>
FastCheckoutClientImpl::CreateFastCheckoutController() {
  return std::make_unique<FastCheckoutControllerImpl>(
      &autofill_client_->GetWebContents(), this);
}

void FastCheckoutClientImpl::OnHidden() {
  fast_checkout_ui_state_ = FastCheckoutUIState::kWasShown;
  keyboard_suppressor_.Unsuppress();
}

void FastCheckoutClientImpl::OnOptionsSelected(
    std::unique_ptr<autofill::AutofillProfile> selected_profile,
    std::unique_ptr<autofill::CreditCard> selected_credit_card) {
  OnHidden();
  selected_autofill_profile_guid_ = selected_profile->guid();
  if (autofill::CreditCard::IsLocalCard(selected_credit_card.get())) {
    selected_credit_card_id_ = selected_credit_card->guid();
    selected_credit_card_is_local_ = true;
  } else {
    selected_credit_card_id_ = selected_credit_card->server_id();
    selected_credit_card_is_local_ = false;
  }
  timeout_timer_.Start(FROM_HERE, kTimeout,
                       base::BindOnce(&FastCheckoutClientImpl::OnRunComplete,
                                      weak_ptr_factory_.GetWeakPtr(),
                                      FastCheckoutRunOutcome::kTimeout,
                                      /*allow_further_runs=*/true));
  TryToFillForms();
  autofill_manager_->TriggerFormExtractionInAllFrames(
      base::BindOnce(&FastCheckoutClientImpl::OnTriggerFormExtractionFinished,
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
  OnRunComplete(FastCheckoutRunOutcome::kBottomsheetDismissed,
                /*allow_further_runs=*/false);
}

void FastCheckoutClientImpl::OnPersonalDataChanged() {
  if (!IsShowing()) {
    return;
  }

  if (trigger_validator_->HasValidPersonalData() !=
      FastCheckoutTriggerOutcome::kSuccess) {
    OnRunComplete(FastCheckoutRunOutcome::kInvalidPersonalData,
                  /*allow_further_runs=*/false);
  } else {
    ShowFastCheckoutUI();
  }
}

bool FastCheckoutClientImpl::AllFormsAreFilled() const {
  return std::ranges::all_of(form_filling_states_,
                             [](const auto& pair) {
                               return pair.second == FillingState::kFilled;
                             }) &&
         std::ranges::all_of(
             form_signatures_to_fill_,
             [&](autofill::FormSignature form_signature) {
               return form_filling_states_.contains(std::make_pair(
                          form_signature, autofill::FormType::kAddressForm)) ||
                      form_filling_states_.contains(std::make_pair(
                          form_signature, autofill::FormType::kCreditCardForm));
             });
}

bool FastCheckoutClientImpl::IsFilling() const {
  return IsRunning() && selected_autofill_profile_guid_ &&
         selected_credit_card_id_;
}

void FastCheckoutClientImpl::OnAfterLoadedServerPredictions(
    autofill::AutofillManager& manager) {
  TryToFillForms();
}

void FastCheckoutClientImpl::OnTriggerFormExtractionFinished(bool success) {
  if (!form_extraction_timer_.IsRunning()) {
    // Trigger form (re-)extraction in all frames continuously until the run
    // stops. That will eventually trigger this
    // (`OnAfterLoadedServerPredictions()`) method.
    form_extraction_timer_.Start(
        FROM_HERE, kSleepBetweenTriggerFormExtractionCalls,
        base::BindOnce(
            &autofill::AutofillManager::TriggerFormExtractionInAllFrames,
            autofill_manager_,
            base::BindOnce(
                &FastCheckoutClientImpl::OnTriggerFormExtractionFinished,
                weak_ptr_factory_.GetWeakPtr())));
  }
}

void FastCheckoutClientImpl::TryToFillForms() {
  if (!IsFilling()) {
    return;
  }
  SetFormFillingStates();
  for (const auto& [form_global_id, form] :
       autofill_manager_->form_structures()) {
    if (ShouldFillForm(*form, autofill::FormType::kAddressForm)) {
      autofill::AutofillField* field =
          GetFieldToFill(form->fields(), /*is_credit_card_form=*/false);
      const autofill::AutofillProfile* autofill_profile =
          GetSelectedAutofillProfile();
      if (field && autofill_profile) {
        form_filling_states_[std::make_pair(form->form_signature(),
                                            autofill::FormType::kAddressForm)] =
            FillingState::kFilling;
        auto* bam = static_cast<autofill::BrowserAutofillManager*>(
            autofill_manager_.get());
        bam->SetFastCheckoutRunId(autofill::FieldTypeGroup::kAddress, run_id_);
        bam->FillOrPreviewProfileForm(
            autofill::mojom::ActionPersistence::kFill, form->ToFormData(),
            *field, *autofill_profile,
            autofill::AutofillTriggerDetails(
                autofill::AutofillTriggerSource::kFastCheckout));
      }
    }

    if (ShouldFillForm(*form, autofill::FormType::kCreditCardForm)) {
      autofill::AutofillField* field =
          GetFieldToFill(form->fields(), /*is_credit_card_form=*/true);
      const autofill::CreditCard* credit_card = GetSelectedCreditCard();
      if (field && !credit_card_form_global_id_ && credit_card) {
        if (autofill::CreditCard::IsLocalCard(credit_card)) {
          FillCreditCardForm(*form, *field, *credit_card, u"");
        } else {
          autofill::CreditCardCvcAuthenticator& cvc_authenticator =
              autofill_client_->GetPaymentsAutofillClient()
                  ->GetCvcAuthenticator();
          credit_card_form_global_id_ = form_global_id;
          cvc_authenticator.GetFullCardRequest()->GetFullCard(
              *credit_card,
              autofill::payments::PaymentsAutofillClient::UnmaskCardReason::
                  kAutofill,
              weak_ptr_factory_.GetWeakPtr(),
              cvc_authenticator.GetAsFullCardRequestUIDelegate());
        }
      }
    }
  }
}

void FastCheckoutClientImpl::FillCreditCardForm(
    const autofill::FormStructure& form,
    const autofill::FormFieldData& field,
    const autofill::CreditCard& credit_card,
    const std::u16string& cvc) {
  form_filling_states_[std::make_pair(form.form_signature(),
                                      autofill::FormType::kCreditCardForm)] =
      FillingState::kFilling;
  auto* bam =
      static_cast<autofill::BrowserAutofillManager*>(autofill_manager_.get());
  bam->SetFastCheckoutRunId(autofill::FieldTypeGroup::kCreditCard, run_id_);
  bam->FillOrPreviewCreditCardForm(
      autofill::mojom::ActionPersistence::kFill, form.ToFormData(), field,
      credit_card, cvc,
      {.trigger_source = autofill::AutofillTriggerSource::kFastCheckout});
}

const autofill::AutofillProfile*
FastCheckoutClientImpl::GetSelectedAutofillProfile() {
  const autofill::AutofillProfile* autofill_profile =
      personal_data_helper_->GetPersonalDataManager()
          ->address_data_manager()
          .GetProfileByGUID(selected_autofill_profile_guid_.value());
  if (!autofill_profile) {
    OnRunComplete(FastCheckoutRunOutcome::kAutofillProfileDeleted);
  }
  return autofill_profile;
}

autofill::CreditCard* FastCheckoutClientImpl::GetSelectedCreditCard() {
  autofill::CreditCard* credit_card = nullptr;
  if (selected_credit_card_is_local_) {
    credit_card = personal_data_helper_->GetPersonalDataManager()
                      ->payments_data_manager()
                      .GetCreditCardByGUID(selected_credit_card_id_.value());
  } else {
    credit_card =
        personal_data_helper_->GetPersonalDataManager()
            ->payments_data_manager()
            .GetCreditCardByServerId(selected_credit_card_id_.value());
  }
  if (!credit_card) {
    OnRunComplete(FastCheckoutRunOutcome::kCreditCardDeleted);
  }
  return credit_card;
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

void FastCheckoutClientImpl::OnFullCardRequestSucceeded(
    const autofill::payments::FullCardRequest& full_card_request,
    const autofill::CreditCard& card,
    const std::u16string& cvc) {
  if (!IsFilling() || !credit_card_form_global_id_) {
    return;
  }
  if (!autofill_manager_->form_structures().contains(
          credit_card_form_global_id_.value())) {
    credit_card_form_global_id_ = std::nullopt;
    return;
  }
  const std::unique_ptr<autofill::FormStructure>& form =
      autofill_manager_->form_structures().at(
          credit_card_form_global_id_.value());
  if (autofill::AutofillField* field =
          GetFieldToFill(form->fields(), /*is_credit_card_form=*/true)) {
    FillCreditCardForm(*form, *field, card, cvc);
  }
  credit_card_form_global_id_ = std::nullopt;
}

void FastCheckoutClientImpl::OnFullCardRequestFailed(
    autofill::CreditCard::RecordType card_type,
    autofill::payments::FullCardRequest::FailureType failure_type) {
  if (!IsFilling() || !credit_card_form_global_id_) {
    return;
  }
  if (failure_type ==
      autofill::payments::FullCardRequest::FailureType::PROMPT_CLOSED) {
    OnRunComplete(FastCheckoutRunOutcome::kCvcPopupClosed,
                  /*allow_further_runs=*/false);
  } else {
    OnRunComplete(FastCheckoutRunOutcome::kCvcPopupError,
                  /*allow_further_runs=*/false);
  }
}

void FastCheckoutClientImpl::OnAfterDidFillAutofillFormData(
    autofill::AutofillManager& manager,
    autofill::FormGlobalId form_id) {
  if (!IsFilling()) {
    return;
  }
  UpdateFillingStates();
  if (AllFormsAreFilled()) {
    OnRunComplete(FastCheckoutRunOutcome::kSuccess,
                  /*allow_further_runs=*/false);
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
      A11yAnnounce(form_signature, /*is_credit_card_form=*/false);
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
        A11yAnnounce(form_signature, /*is_credit_card_form=*/true);
      }
    }
  }
}

void FastCheckoutClientImpl::A11yAnnounce(
    autofill::FormSignature form_signature,
    bool is_credit_card_form) {
  if (is_credit_card_form) {
    if (const autofill::CreditCard* credit_card = GetSelectedCreditCard()) {
      accessibility_service_->Announce(l10n_util::GetStringFUTF16(
          IDS_FAST_CHECKOUT_A11Y_CREDIT_CARD_FORM_FILLED,
          credit_card->HasNonEmptyValidNickname()
              ? credit_card->nickname()
              : credit_card->NetworkAndLastFourDigits()));
    }
    return;
  }

  if (ContainsEmailFormWithSignature(autofill_manager_->form_structures(),
                                     form_signature)) {
    accessibility_service_->Announce(
        l10n_util::GetStringUTF16(IDS_FAST_CHECKOUT_A11Y_EMAIL_FILLED));
  } else if (const autofill::AutofillProfile* autofill_profile =
                 GetSelectedAutofillProfile()) {
    accessibility_service_->Announce(l10n_util::GetStringFUTF16(
        IDS_FAST_CHECKOUT_A11Y_ADDRESS_FORM_FILLED,
        base::UTF8ToUTF16(autofill_profile->profile_label())));
  }
}

void FastCheckoutClientImpl::OnAutofillManagerStateChanged(
    autofill::AutofillManager& manager,
    autofill::AutofillManager::LifecycleState old_state,
    autofill::AutofillManager::LifecycleState new_state) {
  using enum autofill::AutofillManager::LifecycleState;
  switch (new_state) {
    case kInactive:
    case kActive:
      break;
    case kPendingReset:
      if (IsShowing()) {
        OnRunComplete(
            FastCheckoutRunOutcome::kNavigationWhileBottomsheetWasShown);
      } else {
        OnRunComplete(FastCheckoutRunOutcome::kPageRefreshed);
      }
      break;
    case kPendingDeletion:
      if (IsRunning()) {
        if (autofill_client_->GetWebContents().IsBeingDestroyed()) {
          OnRunComplete(FastCheckoutRunOutcome::kTabClosed);
        } else {
          OnRunComplete(FastCheckoutRunOutcome::kAutofillManagerDestroyed);
        }
        return;
      }
      InternalStop(/*allow_further_runs=*/true);
      break;
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

void FastCheckoutClientImpl::OnNavigation(const GURL& url,
                                          bool is_cart_or_checkout_url) {
  if (!IsRunning()) {
    fast_checkout_ui_state_ = FastCheckoutUIState::kNotShownYet;
    return;
  }
  if (url::Origin::Create(url) != origin_) {
    OnRunComplete(FastCheckoutRunOutcome::kOriginChange);
  } else if (!is_cart_or_checkout_url) {
    OnRunComplete(FastCheckoutRunOutcome::kNonCheckoutPage);
  }
}

FastCheckoutTriggerOutcome FastCheckoutClientImpl::CanRun(
    const autofill::FormData& form,
    const autofill::FormFieldData& field,
    const autofill::AutofillManager& autofill_manager) const {
  return trigger_validator_->ShouldRun(form, field, fast_checkout_ui_state_,
                                       is_running_, autofill_manager);
}

bool FastCheckoutClientImpl::IsNotShownYet() const {
  return fast_checkout_ui_state_ == FastCheckoutUIState::kNotShownYet;
}

WEB_CONTENTS_USER_DATA_KEY_IMPL(FastCheckoutClientImpl);
