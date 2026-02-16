// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/containers/to_vector.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/types/optional_ref.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/autofill/autofill_entity_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_ai_util.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/user_education/browser_user_education_interface.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/autofill_ai/entity_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_manager/personal_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_constants.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_utils.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_instance.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/foundations/browser_autofill_manager.h"
#include "components/autofill/core/browser/integrators/autofill_ai/management_utils.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/network/autofill_ai/wallet_pass_access_manager.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/permissions/autofill_ai/autofill_ai_permission_utils.h"
#include "components/autofill/core/browser/studies/autofill_experiments.h"
#include "components/autofill/core/browser/ui/addresses/autofill_address_util.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/autofill_regexes.h"
#include "components/autofill/core/common/dense_set.h"
#include "components/device_reauth/device_authenticator.h"
#include "components/feature_engagement/public/feature_constants.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/wallet/core/browser/walletable_permission_utils.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "third_party/re2/src/re2/re2.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

namespace autofill_private = extensions::api::autofill_private;
namespace addressinput = i18n::addressinput;

using autofill::AddressDataManager;
using autofill::AutofillEntityDataManagerFactory;
using autofill::EntityDataManager;
using autofill::EntityInstance;
using autofill::EntityType;
using autofill::EntityTypeName;
using autofill::PaymentsDataManager;
using autofill::autofill_metrics::LogMandatoryReauthOptInOrOutUpdateEvent;
using autofill::autofill_metrics::LogMandatoryReauthSettingsPageEditCardEvent;
using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;
using autofill::autofill_metrics::MandatoryReauthOptInOrOutSource;

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorPaymentMethodUnavailable[] =
    "Credit card data unavailable";
static const char kErrorAutofillClientUnavailable[] =
    "Autofill client unavailable.";
static const char kErrorAddressDataManagerUnavailable[] =
    "Autofill address data manager unavailable.";
static const char kErrorAddressDataManagerLoadingUnfinished[] =
    "Autofill address data manager loading unfinished.";
static const char kErrorAutofillManagerUnavailable[] =
    "Autofill manager unavailable.";
static const char kErrorPaymentsDataManagerUnavailable[] =
    "Autofill payments data manager unavailable.";
static const char kErrorPaymentsDataManagerLoadingUnfinished[] =
    "Autofill payments data manager loading unfinished.";
static const char kErrorAddressProfileUnavailable[] =
    "Autofill address profile unavailable.";
static const char kErrorAutofillAiUnavailable[] =
    "Autofill AI data unavailable.";
static const char kErrorAutofillAiInvalidData[] =
    "The provided Autofill AI entity/attribute is invalid.";
static const char kErrorAutofillAiTypeNameOutOfBounds[] =
    "The provided Autofill AI entity/attribute type name is out of bounds.";
static const char kErrorAutofillAiEntityInstanceNotFound[] =
    "The provided Autofill AI entity instance cannot be found.";
static const char kErrorDeviceAuthUnavailable[] = "Device auth is unvailable";

// Constant to assign a user-verified verification status to the autofill
// profile.
constexpr auto kUserVerified = autofill::VerificationStatus::kUserVerified;

// Dictionary keys used for serializing AddressUiComponent. Those values
// are used as keys in JavaScript code and shouldn't be modified.
constexpr char kFieldTypeKey[] = "field";
constexpr char kFieldLengthKey[] = "isLongField";
constexpr char kFieldNameKey[] = "fieldName";
constexpr char kFieldRequired[] = "isRequired";

// Serializes the AddressUiComponent a map from string to base::Value().
base::DictValue AddressUiComponentAsValueMap(
    const autofill::AutofillAddressUIComponent& address_ui_component) {
  base::DictValue info;
  info.Set(kFieldNameKey, address_ui_component.name);
  info.Set(kFieldTypeKey, FieldTypeToStringView(address_ui_component.field));
  info.Set(kFieldLengthKey,
           address_ui_component.length_hint ==
               autofill::AutofillAddressUIComponent::HINT_LONG);
  info.Set(kFieldRequired, address_ui_component.is_required);
  return info;
}

bool HasNameSeparator(const std::string& name) {
  if (name.empty()) {
    return false;
  }
  return re2::RE2::PartialMatch(name, autofill::kCjkNameSeparatorsRe);
}

// Logs whether the alternative name in a new/updated profile contains a
// separator.
void RecordAlternativeNameSeparatorUsage(
    const autofill::AutofillProfile& profile,
    const autofill::AutofillProfile* existing_profile) {
  const std::u16string existing_alternative_name =
      existing_profile
          ? existing_profile->GetInfo(autofill::ALTERNATIVE_FULL_NAME,
                                      extensions::ExtensionsBrowserClient::Get()
                                          ->GetApplicationLocale())
          : std::u16string();

  const std::u16string saved_alternative_name = profile.GetInfo(
      autofill::ALTERNATIVE_FULL_NAME,
      extensions::ExtensionsBrowserClient::Get()->GetApplicationLocale());

  if (!saved_alternative_name.empty() &&
      saved_alternative_name != existing_alternative_name) {
    base::UmaHistogramBoolean(
        "Autofill.Settings.EditedAlternativeNameContainsASeparator",
        HasNameSeparator(base::UTF16ToUTF8(saved_alternative_name)));
  }
}

autofill::BrowserAutofillManager* GetBrowserAutofillManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
  if (!autofill_driver) {
    return nullptr;
  }
  // This cast is safe, since `AutofillManager` is always a
  // `BrowserAutofillManager` apart from on WebView.
  return static_cast<autofill::BrowserAutofillManager*>(
      &autofill_driver->GetAutofillManager());
}

autofill::AutofillProfile CreateNewAutofillProfile(
    const autofill::AddressDataManager& adm,
    std::optional<std::string_view> country_code) {
  autofill::AutofillProfile::RecordType record_type =
      adm.IsEligibleForAddressAccountStorage()
          ? autofill::AutofillProfile::RecordType::kAccount
          : autofill::AutofillProfile::RecordType::kLocalOrSyncable;
  autofill::AddressCountryCode address_country_code =
      country_code.has_value()
          ? autofill::AddressCountryCode(std::string(*country_code))
          : autofill::i18n_model_definition::kLegacyHierarchyCountryCode;
  return autofill::AutofillProfile(record_type, address_country_code);
}

// Returns true if the `entity_type` supports wallet storage.
bool IsEligibleForWalletStorage(autofill::AutofillClient* client,
                                autofill::EntityType entity_type) {
  return client &&
         autofill::MayPerformAutofillAiAction(
             *client, autofill::AutofillAiAction::kImportToWallet,
             entity_type) &&
         base::FeatureList::IsEnabled(
             autofill::features::kAutofillEnableSaveToWalletFromSettings);
}

}  // namespace

namespace extensions {

autofill::AddressDataManager*
AutofillPrivateExtensionFunction::address_data_manager() {
  autofill::ContentAutofillClient* client = autofill_client();
  return client ? &client->GetPersonalDataManager().address_data_manager()
                : nullptr;
}

autofill::ContentAutofillClient*
AutofillPrivateExtensionFunction::autofill_client() {
  return autofill::ContentAutofillClient::FromWebContents(
      GetSenderWebContents());
}

autofill::PaymentsDataManager*
AutofillPrivateExtensionFunction::payments_data_manager() {
  autofill::ContentAutofillClient* client = autofill_client();
  return client ? &client->GetPersonalDataManager().payments_data_manager()
                : nullptr;
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAccountInfoFunction

ExtensionFunction::ResponseAction AutofillPrivateGetAccountInfoFunction::Run() {
  AddressDataManager* adm = address_data_manager();
  if (!adm) {
    return RespondNow(Error(base::StrCat(
        {"Get account info - ", kErrorAddressDataManagerUnavailable})));
  }

  if (!adm->has_initial_load_finished()) {
    return RespondNow(Error(base::StrCat(
        {"Get account info - ", kErrorAddressDataManagerLoadingUnfinished})));
  }

  std::optional<api::autofill_private::AccountInfo> account_info =
      autofill_util::GetAccountInfo(*adm);
  if (account_info.has_value()) {
    return RespondNow(
        ArgumentList(api::autofill_private::GetAccountInfo::Results::Create(
            account_info.value())));
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveAddressFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveAddressFunction::Run() {
  std::optional<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  AddressDataManager* adm = address_data_manager();
  if (!adm) {
    return RespondNow(Error(base::StrCat(
        {"Save address - ", kErrorAddressDataManagerUnavailable})));
  }

  if (!adm->has_initial_load_finished()) {
    return RespondNow(Error(base::StrCat(
        {"Save address - ", kErrorAddressDataManagerLoadingUnfinished})));
  }

  // If a profile guid is specified, get a copy of the profile identified by it.
  // Otherwise create a new one.
  api::autofill_private::AddressEntry* address = &parameters->address;
  std::string guid = address->guid ? *address->guid : "";
  const bool use_existing_profile = !guid.empty();
  const autofill::AutofillProfile* existing_profile = nullptr;
  if (use_existing_profile) {
    existing_profile = adm->GetProfileByGUID(guid);
    if (!existing_profile) {
      return RespondNow(Error(
          base::StrCat({"Save address - ", kErrorAddressProfileUnavailable})));
    }
  }
  std::optional<std::string_view> country_code;
  if (auto it = std::find_if(
          address->fields.begin(), address->fields.end(),
          [](const auto& field) {
            return field.type ==
                   autofill_private::FieldType::kAddressHomeCountry;
          });
      it != address->fields.end()) {
    country_code = it->value;
  }
  autofill::AutofillProfile profile =
      existing_profile ? *existing_profile
                       : CreateNewAutofillProfile(*adm, country_code);

  for (const api::autofill_private::AddressField& field : address->fields) {
    std::u16string trimmed_value;
    base::TrimWhitespace(base::UTF8ToUTF16(field.value), base::TRIM_ALL,
                         &trimmed_value);
    // TODO(crbug.com/385727960): Investigate why we can't use
    // SetInfoWithVerificationStatus here.
    profile.SetRawInfoWithVerificationStatus(
        autofill::TypeNameToFieldType(autofill_private::ToString(field.type)),
        trimmed_value, kUserVerified);
  }
  profile.FinalizeAfterImport();

  RecordAlternativeNameSeparatorUsage(profile, existing_profile);

  if (address->language_code) {
    profile.set_language_code(*address->language_code);
  }

  if (use_existing_profile) {
    adm->UpdateProfile(profile);
  } else {
    adm->AddProfile(profile);
    autofill::autofill_metrics::LogManuallyAddedAddress(
        autofill::autofill_metrics::AutofillManuallyAddedAddressSurface::
            kSettings);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveAddressFunction

ExtensionFunction::ResponseAction AutofillPrivateRemoveAddressFunction::Run() {
  std::optional<api::autofill_private::RemoveAddress::Params> parameters =
      api::autofill_private::RemoveAddress::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  AddressDataManager* adm = address_data_manager();
  if (!adm) {
    return RespondNow(Error(base::StrCat(
        {"Remove address - ", kErrorAddressDataManagerUnavailable})));
  }
  if (!adm->has_initial_load_finished()) {
    return RespondNow(Error(base::StrCat(
        {"Remove address - ", kErrorAddressDataManagerLoadingUnfinished})));
  }
  adm->RemoveProfile(parameters->guid);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  std::optional<api::autofill_private::GetCountryList::Params> parameters =
      api::autofill_private::GetCountryList::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill_util::CountryEntryList country_list;
  if (parameters->for_account_storage) {
    AddressDataManager* adm = address_data_manager();
    if (!adm) {
      return RespondNow(Error(base::StrCat(
          {"Get country list - ", kErrorAddressDataManagerUnavailable})));
    }

    // Return an empty list if data is not loaded.
    if (!adm->has_initial_load_finished()) {
      return RespondNow(
          ArgumentList(api::autofill_private::GetCountryList::Results::Create(
              country_list)));
    }
  }
  country_list = autofill_util::GenerateCountryList();
  return RespondNow(ArgumentList(
      api::autofill_private::GetCountryList::Results::Create(country_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressComponentsFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetAddressComponentsFunction::Run() {
  std::optional<api::autofill_private::GetAddressComponents::Params>
      parameters =
          api::autofill_private::GetAddressComponents::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  std::vector<std::vector<autofill::AutofillAddressUIComponent>> lines;
  std::string language_code;

  autofill::GetAddressComponents(
      parameters->country_code,
      ExtensionsBrowserClient::Get()->GetApplicationLocale(),
      /*include_literals=*/false, &lines, &language_code);
  // Convert std::vector<std::vector<::i18n::addressinput::AddressUiComponent>>
  // to AddressComponents
  base::DictValue address_components;
  base::ListValue rows;

  for (auto& line : lines) {
    base::ListValue row_values;
    for (const autofill::AutofillAddressUIComponent& component : line) {
      row_values.Append(AddressUiComponentAsValueMap(component));
    }
    base::DictValue row;
    row.Set("row", std::move(row_values));
    rows.Append(std::move(row));
  }

  address_components.Set("components", std::move(rows));
  address_components.Set("languageCode", language_code);

  return RespondNow(WithArguments(std::move(address_components)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetAddressListFunction::Run() {
  AddressDataManager* adm = address_data_manager();
  if (!adm) {
    return RespondNow(Error(base::StrCat(
        {"Get address list - ", kErrorAddressDataManagerUnavailable})));
  }
  if (!adm->has_initial_load_finished()) {
    return RespondNow(Error(base::StrCat(
        {"Get address list - ", kErrorAddressDataManagerLoadingUnfinished})));
  }
  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*adm);
  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  std::optional<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Save credit card - ", kErrorPaymentsDataManagerUnavailable})));
  }

  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(base::StrCat(
        {"Save credit card - ", kErrorPaymentsDataManagerLoadingUnfinished})));
  }

  // If a card guid is specified, get a copy of the card identified by it.
  // Otherwise create a new one.
  api::autofill_private::CreditCardEntry* card = &parameters->card;
  std::string guid = card->guid ? *card->guid : "";
  const bool use_existing_card = !guid.empty();
  const autofill::CreditCard* existing_card = nullptr;
  if (use_existing_card) {
    existing_card = paydm->GetCreditCardByGUID(guid);
    if (!existing_card) {
      return RespondNow(Error(base::StrCat(
          {"Save credit card - ", kErrorPaymentMethodUnavailable})));
    }
  }
  autofill::CreditCard credit_card =
      existing_card ? *existing_card
                    : autofill::CreditCard(
                          base::Uuid::GenerateRandomV4().AsLowercaseString(),
                          kSettingsOrigin);

  if (card->name) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NAME_FULL,
                           base::UTF8ToUTF16(*card->name));
  }

  if (card->card_number) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_NUMBER,
                           base::UTF8ToUTF16(*card->card_number));
  }

  if (card->expiration_month) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_MONTH,
                           base::UTF8ToUTF16(*card->expiration_month));
  }

  if (card->expiration_year) {
    credit_card.SetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
                           base::UTF8ToUTF16(*card->expiration_year));
  }

  if (card->nickname) {
    credit_card.SetNickname(base::UTF8ToUTF16(*card->nickname));
  }

  if (card->cvc) {
    credit_card.set_cvc(base::UTF8ToUTF16(*card->cvc));
  }

  if (use_existing_card) {
    // Only updates when the card info changes.
    if (existing_card && existing_card->Compare(credit_card) == 0) {
      return RespondNow(NoArguments());
    }

    if (existing_card->cvc().empty()) {
      if (credit_card.cvc().empty()) {
        // Record when an existing card without CVC is edited and no CVC was
        // added.
        base::RecordAction(base::UserMetricsAction(
            "AutofillCreditCardsEditedAndCvcWasLeftBlank"));
      } else {
        // Record when an existing card without CVC is edited and CVC was added.
        base::RecordAction(
            base::UserMetricsAction("AutofillCreditCardsEditedAndCvcWasAdded"));
      }
    } else {
      if (credit_card.cvc().empty()) {
        // Record when an existing card with CVC is edited and CVC was removed.
        base::RecordAction(base::UserMetricsAction(
            "AutofillCreditCardsEditedAndCvcWasRemoved"));
      } else if (credit_card.cvc() != existing_card->cvc()) {
        // Record when an existing card with CVC is edited and CVC was updated.
        base::RecordAction(base::UserMetricsAction(
            "AutofillCreditCardsEditedAndCvcWasUpdated"));
      } else {
        // Record when an existing card with CVC is edited and CVC was
        // unchanged.
        base::RecordAction(base::UserMetricsAction(
            "AutofillCreditCardsEditedAndCvcWasUnchanged"));
      }
    }

    // Record when nickname is updated.
    if (credit_card.HasNonEmptyValidNickname() &&
        existing_card->nickname() != credit_card.nickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsEditedWithNickname"));
    }

    paydm->UpdateCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsEdited"));
  } else {
    int current_card_count = paydm->GetCreditCards().size();
    paydm->AddCreditCard(credit_card);

    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsAdded"));

    base::UmaHistogramCounts100(
        "Autofill.PaymentMethods.SettingsPage."
        "StoredCreditCardCountBeforeCardAdded",
        current_card_count);
    base::UmaHistogramBoolean(
        "Autofill.PaymentMethodsSettingsPage.CardAddedWithoutExistingCards",
        current_card_count == 0);

    if (credit_card.HasNonEmptyValidNickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsAddedWithNickname"));
    }
    if (!credit_card.cvc().empty()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsAddedWithCvc"));
    }
  }
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemovePaymentsEntityFunction

ExtensionFunction::ResponseAction
AutofillPrivateRemovePaymentsEntityFunction::Run() {
  std::optional<api::autofill_private::RemovePaymentsEntity::Params>
      parameters =
          api::autofill_private::RemovePaymentsEntity::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Remove payments entity - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Remove payments entity - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  if (paydm->GetIbanByGUID(parameters->guid)) {
    base::RecordAction(base::UserMetricsAction("AutofillIbanDeleted"));
  } else if (const autofill::CreditCard* credit_card =
                 paydm->GetCreditCardByGUID(parameters->guid)) {
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardDeleted"));
    if (!credit_card->cvc().empty()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardDeletedAndHadCvc"));
    }
    if (credit_card->HasNonEmptyValidNickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardDeletedAndHadNickname"));
    }
  }

  paydm->RemoveByGUID(parameters->guid);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Get credit card list - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Get credit card list - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  autofill_util::CreditCardEntryList credit_card_list =
      autofill_util::GenerateCreditCardList(*paydm);
  return RespondNow(
      ArgumentList(api::autofill_private::GetCreditCardList::Results::Create(
          credit_card_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLogServerCardLinkClickedFunction

ExtensionFunction::ResponseAction
AutofillPrivateLogServerCardLinkClickedFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(
        Error(base::StrCat({"Log server card link clicked - ",
                            kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Log server card link clicked - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  paydm->LogServerCardLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLogServerIbanLinkClickedFunction

ExtensionFunction::ResponseAction
AutofillPrivateLogServerIbanLinkClickedFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(
        Error(base::StrCat({"Log server iban link clicked - ",
                            kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Log server iban link clicked - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  paydm->LogServerIbanLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveIbanFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveIbanFunction::Run() {
  std::optional<api::autofill_private::SaveIban::Params> parameters =
      api::autofill_private::SaveIban::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(
        base::StrCat({"Save iban - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(base::StrCat(
        {"Save iban - ", kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  api::autofill_private::IbanEntry* iban_entry = &parameters->iban;
  CHECK(iban_entry->value);
  const autofill::Iban* existing_iban = nullptr;

  // The IBAN guid is specified if the user tries to update an existing IBAN via
  // the Chrome payment settings page.
  if (iban_entry->guid.has_value() && !iban_entry->guid->empty()) {
    existing_iban = paydm->GetIbanByGUID(*iban_entry->guid);
    CHECK(existing_iban);
  }

  autofill::Iban iban_to_write =
      existing_iban ? *existing_iban : autofill::Iban();

  iban_to_write.set_value(base::UTF8ToUTF16(*iban_entry->value));

  if (iban_entry->nickname) {
    iban_to_write.set_nickname(base::UTF8ToUTF16(*iban_entry->nickname));
  }

  // Add a new IBAN and return if this is not an update.
  if (!existing_iban) {
    paydm->AddAsLocalIban(iban_to_write);
    base::RecordAction(base::UserMetricsAction("AutofillIbanAdded"));
    if (!iban_to_write.nickname().empty()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillIbanAddedWithNickname"));
    }
    return RespondNow(NoArguments());
  }

  // This is an existing IBAN. Update the database entry in case anything has
  // changed.
  if (existing_iban->Compare(iban_to_write) != 0) {
    bool nickname_changed =
        existing_iban->nickname() != iban_to_write.nickname();
    paydm->UpdateIban(iban_to_write);
    base::RecordAction(base::UserMetricsAction("AutofillIbanEdited"));
    if (nickname_changed) {
      base::RecordAction(
          base::UserMetricsAction("AutofillIbanEditedWithNickname"));
    }
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetIbanListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetIbanListFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Get iban list - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(base::StrCat(
        {"Get iban list - ", kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  autofill_util::IbanEntryList iban_list =
      autofill_util::GenerateIbanList(*paydm);
  return RespondNow(ArgumentList(
      api::autofill_private::GetIbanList::Results::Create(iban_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateIsValidIbanFunction

ExtensionFunction::ResponseAction AutofillPrivateIsValidIbanFunction::Run() {
  std::optional<api::autofill_private::IsValidIban::Params> parameters =
      api::autofill_private::IsValidIban::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  return RespondNow(WithArguments(
      autofill::Iban::IsValid(base::UTF8ToUTF16(parameters->iban_value))));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAddVirtualCardFunction

ExtensionFunction::ResponseAction AutofillPrivateAddVirtualCardFunction::Run() {
  std::optional<api::autofill_private::AddVirtualCard::Params> parameters =
      api::autofill_private::AddVirtualCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Add virtual card - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(base::StrCat(
        {"Add virtual card - ", kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  const autofill::CreditCard* card =
      paydm->GetCreditCardByServerId(parameters->card_id);
  if (!card) {
    return RespondNow(Error(
        base::StrCat({"Add virtual card - ", kErrorPaymentMethodUnavailable})));
  }

  auto* virtual_card_enrollment_manager =
      autofill_client()
          ->GetPaymentsAutofillClient()
          ->GetVirtualCardEnrollmentManager();
  CHECK(virtual_card_enrollment_manager);
  virtual_card_enrollment_manager->InitVirtualCardEnroll(
      *card, autofill::VirtualCardEnrollmentSource::kSettingsPage,
      base::BindOnce(
          &autofill::VirtualCardEnrollmentManager::ShowVirtualCardEnrollBubble,
          base::Unretained(virtual_card_enrollment_manager)));
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveVirtualCardFunction

ExtensionFunction::ResponseAction
AutofillPrivateRemoveVirtualCardFunction::Run() {
  std::optional<api::autofill_private::RemoveVirtualCard::Params> parameters =
      api::autofill_private::RemoveVirtualCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Remove virtual card - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Remove virtual card - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  const autofill::CreditCard* card =
      paydm->GetCreditCardByServerId(parameters->card_id);
  if (!card) {
    return RespondNow(Error(base::StrCat(
        {"Remove virtual card - ", kErrorPaymentMethodUnavailable})));
  }

  autofill::BrowserAutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager) {
    return RespondNow(Error(base::StrCat(
        {"Remove virtual card - ", kErrorAutofillManagerUnavailable})));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          .GetPaymentsAutofillClient()
          ->GetVirtualCardEnrollmentManager();

  virtual_card_enrollment_manager->Unenroll(
      card->instrument_id(),
      /*virtual_card_enrollment_update_response_callback=*/std::nullopt);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetPayOverTimeIssuerListFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetPayOverTimeIssuerListFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(
        Error(base::StrCat({"Get pay over time issuer list - ",
                            kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Get pay over time issuer list - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  autofill_util::PayOverTimeIssuerEntryList pay_over_time_issuer_list =
      autofill_util::GeneratePayOverTimeIssuerList(*paydm);
  return RespondNow(ArgumentList(
      api::autofill_private::GetPayOverTimeIssuerList::Results::Create(
          pay_over_time_issuer_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction

ExtensionFunction::ResponseAction
AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(
        base::StrCat({"Authenticate user and flip mandatory auth toggle - ",
                      kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(
        base::StrCat({"Authenticate user and flip mandatory auth toggle - ",
                      kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  // We will be modifying the pref `kAutofillPaymentMethodsMandatoryReauth`
  // asynchronously. The pref value directly correlates to the mandatory auth
  // toggle.
  // We are also logging the start of the auth flow and
  // `!IsPaymentMethodsMandatoryReauthEnabled()` denotes that the user is either
  // opting in or out.
  base::RecordAction(base::UserMetricsAction(
      "PaymentsUserAuthTriggeredForMandatoryAuthToggle"));
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage,
      /*opt_in=*/!paydm->IsPaymentMethodsMandatoryReauthEnabled(),
      MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
  autofill_client()
      ->GetPaymentsAutofillClient()
      ->GetOrCreatePaymentsMandatoryReauthManager()
      ->AuthenticateWithMessage(
          l10n_util::GetStringUTF16(
              IDS_PAYMENTS_AUTOFILL_MANDATORY_REAUTH_PROMPT),
          base::BindOnce(
              &AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::
                  UpdateMandatoryAuthTogglePref,
              this));

  return RespondNow(NoArguments());
#else
  return RespondNow(
      Error(base::StrCat({"Authenticate user and flip mandatory auth toggle - ",
                          kErrorDeviceAuthUnavailable})));
#endif  // BUILDFLAG (IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
}

// Update the Mandatory auth toggle pref and log whether the auth was successful
// or not.
void AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::
    UpdateMandatoryAuthTogglePref(bool reauth_succeeded) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return;
  }

  // `opt_in` bool denotes whether the user is trying to opt in or out of the
  // mandatory reauth feature. If the mandatory reauth toggle on the settings is
  // currently enabled, then the `opt_in` bool will be false because the user is
  // opting-out, otherwise the `opt_in` bool will be true.
  const bool opt_in = !paydm->IsPaymentMethodsMandatoryReauthEnabled();
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage, opt_in,
      reauth_succeeded ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
                       : MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
  if (reauth_succeeded) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthSuccessfulForMandatoryAuthToggle"));
    paydm->SetPaymentMethodsMandatoryReauthEnabled(opt_in);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetLocalCardFunction

ExtensionFunction::ResponseAction AutofillPrivateGetLocalCardFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Get local card - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(Error(base::StrCat(
        {"Get local card - ", kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  if (paydm->IsPaymentMethodsMandatoryReauthEnabled()) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
    LogMandatoryReauthSettingsPageEditCardEvent(
        MandatoryReauthAuthenticationFlowEvent::kFlowStarted);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
    // Based on the result of the auth, we will be asynchronously returning the
    // card if the user can edit the local card.
    autofill_client()
        ->GetPaymentsAutofillClient()
        ->GetOrCreatePaymentsMandatoryReauthManager()
        ->AuthenticateWithMessage(
            l10n_util::GetStringUTF16(
                IDS_PAYMENTS_AUTOFILL_EDIT_CARD_MANDATORY_REAUTH_PROMPT),
            base::BindOnce(
                &AutofillPrivateGetLocalCardFunction::OnReauthFinished, this));
#else
    // This Autofill private API is only available on desktop systems and
    // IsPaymentMethodsMandatoryReauthEnabled() ensures that it's only enabled
    // for MacOS and Windows.
    NOTREACHED();
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  } else {
    ReturnCreditCard();
  }
  // Due to async nature of AuthenticateWithMessage() on mandatory re-auth
  // manager and delayed return on ReturnCreditCard(), we use the below check to
  // make sure we have a `Respond` captured. If we didn't have this check, then
  // we would show the edit card dialog box even before the user successfully
  // completes the auth.
  return did_respond() ? AlreadyResponded() : RespondLater();
}

// This is triggered after the reauth is completed and a local card may be
// returned based on the auth result. We also log whether the auth was
// successful or not.
void AutofillPrivateGetLocalCardFunction::OnReauthFinished(bool can_retrieve) {
  if (!can_retrieve) {
    LogMandatoryReauthSettingsPageEditCardEvent(
        MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
    Respond(NoArguments());
    return;
  }
  base::RecordAction(base::UserMetricsAction(
      "PaymentsUserAuthSuccessfulToShowEditLocalCardDialog"));
  LogMandatoryReauthSettingsPageEditCardEvent(
      MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded);
  ReturnCreditCard();
}

void AutofillPrivateGetLocalCardFunction::ReturnCreditCard() {
  PaymentsDataManager* paydm = payments_data_manager();
  CHECK(paydm);

  std::optional<autofill_private::GetLocalCard::Params> parameters =
      autofill_private::GetLocalCard::Params::Create(args());
  if (auto* card_from_guid = paydm->GetCreditCardByGUID(parameters->guid)) {
    return Respond(ArgumentList(autofill_private::GetLocalCard::Results::Create(
        autofill_util::CreditCardToCreditCardEntry(
            *card_from_guid, *paydm,
            /*mask_local_cards=*/false))));
  }
  return Respond(Error(
      base::StrCat({"Get local card - ", kErrorPaymentMethodUnavailable})));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateCheckIfDeviceAuthAvailableFunction

ExtensionFunction::ResponseAction
AutofillPrivateCheckIfDeviceAuthAvailableFunction::Run() {
  // TODO(crbug.com/467173735): Check with ChromeOS team on the implementation
  // details. It is still in active discussion with the ChromeOS team on how to
  // implement this.
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (client) {
    return RespondNow(WithArguments(autofill::IsDeviceAuthAvailable(
        client->GetDeviceAuthenticator().get())));
  }
#endif  // BUILDFLAG (IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  return RespondNow(Error(base::StrCat(
      {"Check if device auth available - ", kErrorDeviceAuthUnavailable})));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateBulkDeleteAllCvcsFunction

ExtensionFunction::ResponseAction
AutofillPrivateBulkDeleteAllCvcsFunction::Run() {
  PaymentsDataManager* paydm = payments_data_manager();
  if (!paydm) {
    return RespondNow(Error(base::StrCat(
        {"Bulk delete all cvcs - ", kErrorPaymentsDataManagerUnavailable})));
  }
  if (!paydm->is_payments_data_loaded()) {
    return RespondNow(
        Error(base::StrCat({"Bulk delete all cvcs - ",
                            kErrorPaymentsDataManagerLoadingUnfinished})));
  }
  // Clear local and server CVCs from the webdata database. For server CVCs,
  // this will also clear them from the Chrome sync server and thus other
  // devices.
  paydm->ClearLocalCvcs();
  paydm->ClearServerCvcs();

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetAutofillSyncToggleEnabledFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetAutofillSyncToggleEnabledFunction::Run() {
  AddressDataManager* adm = address_data_manager();
  if (!adm) {
    return RespondNow(
        Error(base::StrCat({"Set autofill sync toggle enabled - ",
                            kErrorAddressDataManagerUnavailable})));
  }
  if (!adm->has_initial_load_finished()) {
    return RespondNow(
        Error(base::StrCat({"Set autofill sync toggle enabled - ",
                            kErrorAddressDataManagerLoadingUnfinished})));
  }
  std::optional<api::autofill_private::SetAutofillSyncToggleEnabled::Params>
      parameters =
          api::autofill_private::SetAutofillSyncToggleEnabled::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  adm->SetAutofillSelectableTypeEnabled(parameters->enabled);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAddOrUpdateEntityInstanceFunction

ExtensionFunction::ResponseAction
AutofillPrivateAddOrUpdateEntityInstanceFunction::Run() {
  std::optional<autofill_private::AddOrUpdateEntityInstance::Params>
      parameters =
          autofill_private::AddOrUpdateEntityInstance::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  const autofill_private::EntityInstance& private_api_entity_instance =
      parameters->entity_instance;
  std::optional<EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(
          private_api_entity_instance.type.type_name);

  if (!entity_type_name.has_value()) {
    return RespondNow(
        Error(base::StrCat({"Add or update entity instance - ",
                            kErrorAutofillAiTypeNameOutOfBounds})));
  }

  const bool is_eligible_for_wallet_storage = IsEligibleForWalletStorage(
      autofill_client(), EntityType(*entity_type_name));

  std::optional<EntityInstance> entity_instance =
      autofill_ai_util::PrivateApiEntityInstanceToEntityInstance(
          private_api_entity_instance,
          g_browser_process->GetApplicationLocale(),
          is_eligible_for_wallet_storage);
  if (!entity_instance.has_value()) {
    return RespondNow(Error(base::StrCat(
        {"Add or update entity instance - ", kErrorAutofillAiInvalidData})));
  }

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EntityDataManager* entity_data_manager =
      profile ? AutofillEntityDataManagerFactory::GetForProfile(profile)
              : nullptr;

  if (!entity_data_manager) {
    return RespondNow(Error(base::StrCat(
        {"Add or update entity instance - ", kErrorAutofillAiUnavailable})));
  }

  if (IsMaskedStorageSupported(entity_instance->type(),
                               entity_instance->record_type())) {
    // If the request is successfully started, the callback will handle the
    // response.
    if (TrySavePrivatePassWithWalletAPI(*entity_instance)) {
      return RespondLater();
    }

    SavePrivatePassLocallyAndNotifyAsFallback(*entity_data_manager,
                                              std::move(*entity_instance));
    return RespondNow(NoArguments());
  }

  // Handles the following scenarios:
  // 1. Save entity locally.
  // 2. Save entity to Wallet via Chrome sync.
  entity_data_manager->AddOrUpdateEntityInstance(entity_instance.value());
  if (private_api_entity_instance.stored_in_wallet.value_or(false) &&
      !is_eligible_for_wallet_storage && autofill_client()) {
    autofill_client()->ShowAutofillAiLocalSaveNotification();
  }
  return RespondNow(NoArguments());
}

bool AutofillPrivateAddOrUpdateEntityInstanceFunction::
    TrySavePrivatePassWithWalletAPI(const EntityInstance& entity_instance) {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiWalletPrivatePasses)) {
    return false;
  }

  if (!autofill_client()) {
    return false;
  }

  if (autofill::WalletPassAccessManager* pass_manager =
          autofill_client()->GetWalletPassAccessManager()) {
    pass_manager->SaveWalletEntityInstance(
        entity_instance,
        base::BindOnce(&AutofillPrivateAddOrUpdateEntityInstanceFunction::
                           OnSavePrivatePassToWalletFinished,
                       base::RetainedRef(this), entity_instance));
    return true;
  }

  return false;
}

void AutofillPrivateAddOrUpdateEntityInstanceFunction::
    OnSavePrivatePassToWalletFinished(
        autofill::EntityInstance original_entity,
        std::optional<EntityInstance> saved_entity) {
  content::BrowserContext* context = browser_context();
  if (!context) {
    Respond(Error(kErrorAutofillAiUnavailable));
    return;
  }
  Profile* profile = Profile::FromBrowserContext(context);
  EntityDataManager* entity_data_manager =
      profile ? AutofillEntityDataManagerFactory::GetForProfile(profile)
              : nullptr;

  if (!entity_data_manager) {
    Respond(Error(kErrorAutofillAiUnavailable));
    return;
  }
  if (saved_entity.has_value()) {
    entity_data_manager->AddOrUpdateEntityInstance(std::move(*saved_entity));
  } else {
    SavePrivatePassLocallyAndNotifyAsFallback(*entity_data_manager,
                                              std::move(original_entity));
  }
  Respond(NoArguments());
}

void AutofillPrivateAddOrUpdateEntityInstanceFunction::
    SavePrivatePassLocallyAndNotifyAsFallback(
        EntityDataManager& entity_data_manager,
        EntityInstance entity) {
  EntityInstance local_entity =
      entity.CopyWithNewRecordType(EntityInstance::RecordType::kLocal);
  entity_data_manager.AddOrUpdateEntityInstance(std::move(local_entity));

  // Notify the user.
  if (autofill_client()) {
    autofill_client()->ShowAutofillAiLocalSaveNotification();
  }
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntityInstanceFunction

ExtensionFunction::ResponseAction
AutofillPrivateRemoveEntityInstanceFunction::Run() {
  std::optional<autofill_private::RemoveEntityInstance::Params> parameters =
      autofill_private::RemoveEntityInstance::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EntityDataManager* entity_data_manager =
      profile ? AutofillEntityDataManagerFactory::GetForProfile(profile)
              : nullptr;

  if (!entity_data_manager) {
    return RespondNow(Error(base::StrCat(
        {"Remove entity instance - ", kErrorAutofillAiUnavailable})));
  }
  entity_data_manager->RemoveEntityInstance(
      EntityInstance::EntityId(parameters->guid));
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLoadEntityInstancesFunction

ExtensionFunction::ResponseAction
AutofillPrivateLoadEntityInstancesFunction::Run() {
  Profile* profile = Profile::FromBrowserContext(browser_context());
  EntityDataManager* entity_data_manager =
      profile ? AutofillEntityDataManagerFactory::GetForProfile(profile)
              : nullptr;

  if (!entity_data_manager) {
    return RespondNow(Error(base::StrCat(
        {"Load entity instances - ", kErrorAutofillAiUnavailable})));
  }
  const bool obfuscate_sensitive_types =
      autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
          autofill_client()->GetPrefs());
  std::vector<autofill_private::EntityInstanceWithLabels> result =
      autofill_ai_util::EntityInstancesToPrivateApiEntityInstancesWithLabels(
          entity_data_manager->GetEntityInstances(), obfuscate_sensitive_types,
          g_browser_process->GetApplicationLocale());
  return RespondNow(ArgumentList(
      autofill_private::LoadEntityInstances::Results::Create(result)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetEntityInstanceByGuidFunction

AutofillPrivateGetEntityInstanceByGuidFunction::
    AutofillPrivateGetEntityInstanceByGuidFunction() = default;

AutofillPrivateGetEntityInstanceByGuidFunction::
    ~AutofillPrivateGetEntityInstanceByGuidFunction() = default;

ExtensionFunction::ResponseAction
AutofillPrivateGetEntityInstanceByGuidFunction::Run() {
  std::optional<autofill_private::GetEntityInstanceByGuid::Params> parameters =
      autofill_private::GetEntityInstanceByGuid::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  Profile* profile = Profile::FromBrowserContext(browser_context());
  EntityDataManager* entity_data_manager =
      profile ? AutofillEntityDataManagerFactory::GetForProfile(profile)
              : nullptr;

  if (!entity_data_manager) {
    return RespondNow(Error(base::StrCat(
        {"Get entity instance by guid - ", kErrorAutofillAiUnavailable})));
  }
  base::optional_ref<const EntityInstance> entity_instance =
      entity_data_manager->GetEntityInstance(
          EntityInstance::EntityId(parameters->guid));
  if (!entity_instance.has_value()) {
    return RespondNow(
        Error(base::StrCat({"Get entity instance by guid - ",
                            kErrorAutofillAiEntityInstanceNotFound})));
  }

  authenticator_.reset();
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiReauthRequired)) {
    authenticator_ = autofill_client()->GetDeviceAuthenticator(
        /*histogram=*/"Autofill.Ai.Reauth.ViewEntity");
  }

  const bool should_authenticate_to_view =
      autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
          autofill_client()->GetPrefs()) &&
      authenticator_ &&
      authenticator_->CanAuthenticateWithBiometricOrScreenLock() &&
      std::ranges::any_of(
          entity_instance->attributes(),
          [](const autofill::AttributeInstance& attribute_instance) {
            return attribute_instance.type().is_obfuscated() &&
                   !attribute_instance.GetCompleteRawInfo().empty();
          });

  if (!should_authenticate_to_view) {
    return RespondNow(ArgumentList(
        api::autofill_private::GetEntityInstanceByGuid::Results::Create(
            autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
                entity_instance.value(), autofill_client()->GetAppLocale(),
                IsEligibleForWalletStorage(autofill_client(),
                                           entity_instance.value().type())))));
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  message = l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_VIEWING_REAUTH);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  authenticator_->AuthenticateWithMessage(
      message,
      base::BindOnce(
          &AutofillPrivateGetEntityInstanceByGuidFunction::OnReauthCompleted,
          base::RetainedRef(this), entity_instance.value()));
  return RespondLater();
}

void AutofillPrivateGetEntityInstanceByGuidFunction::OnReauthCompleted(
    const autofill::EntityInstance& entity_instance,
    bool auth_succeeded) {
  if (auth_succeeded) {
    Respond(ArgumentList(
        api::autofill_private::GetEntityInstanceByGuid::Results::Create(
            autofill_ai_util::EntityInstanceToPrivateApiEntityInstance(
                entity_instance, autofill_client()->GetAppLocale(),
                IsEligibleForWalletStorage(autofill_client(),
                                           entity_instance.type())))));
    return;
  }
  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetWritableEntityTypesFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetWritableEntityTypesFunction::Run() {
  auto all_types = autofill::DenseSet<EntityType>::all();

  std::vector<autofill_private::EntityType> result;
  result.reserve(all_types.size());
  for (EntityType entity_type : autofill::GetWritableEntityTypes(
           autofill_client()->GetVariationConfigCountryCode())) {
    result.push_back(autofill_ai_util::EntityTypeToPrivateApiEntityType(
        entity_type,
        IsEligibleForWalletStorage(autofill_client(), entity_type)));
  }

  return RespondNow(ArgumentList(
      autofill_private::GetWritableEntityTypes::Results::Create(result)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetAllAttributeTypesForEntityTypeNameFunction::Run() {
  std::optional<autofill_private::GetAllAttributeTypesForEntityTypeName::Params>
      parameters = autofill_private::GetAllAttributeTypesForEntityTypeName::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  std::optional<EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(parameters->entity_type_name);
  if (!entity_type_name.has_value()) {
    return RespondNow(
        Error(base::StrCat({"Get all attribute types for entity type name - ",
                            kErrorAutofillAiTypeNameOutOfBounds})));
  }

  EntityType entity_type(entity_type_name.value());
  std::vector<autofill_private::AttributeType> result =
      base::ToVector(entity_type.attributes(),
                     autofill_ai_util::AttributeTypeToPrivateApiAttributeType);
  return RespondNow(ArgumentList(
      autofill_private::GetAllAttributeTypesForEntityTypeName::Results::Create(
          result)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetRequiredAttributeTypesForEntityTypeNameFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetRequiredAttributeTypesForEntityTypeNameFunction::Run() {
  const auto params = api::autofill_private::
      GetRequiredAttributeTypesForEntityTypeName::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  std::optional<autofill::EntityTypeName> entity_type_name =
      autofill::ToSafeEntityTypeName(params->entity_type_name);

  if (!entity_type_name.has_value()) {
    return RespondNow(Error(
        base::StrCat({"Get required attribute types for entity type name - ",
                      kErrorAutofillAiTypeNameOutOfBounds})));
  }

  autofill::EntityType entity_type(entity_type_name.value());
  return RespondNow(ArgumentList(
      api::autofill_private::GetRequiredAttributeTypesForEntityTypeName::
          Results::Create(
              autofill_ai_util::GetRequiredAttributesForType(entity_type))));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAutofillAiOptInStatusFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetAutofillAiOptInStatusFunction::Run() {
  return RespondNow(ArgumentList(
      api::autofill_private::GetAutofillAiOptInStatus::Results::Create(
          autofill::GetAutofillAiOptInStatus(*autofill_client()))));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetAutofillAiOptInStatusFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetAutofillAiOptInStatusFunction::Run() {
  std::optional<autofill_private::SetAutofillAiOptInStatus::Params> parameters =
      autofill_private::SetAutofillAiOptInStatus::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  using enum autofill::AutofillAiOptInStatus;
  if (!autofill::SetAutofillAiOptInStatus(
          *autofill_client(), parameters->opted_in ? kOptedIn : kOptedOut)) {
    return RespondNow(ArgumentList(
        api::autofill_private::SetAutofillAiOptInStatus::Results::Create(
            /*success=*/false)));
  }

  if (parameters->opted_in) {
    autofill_client()->NotifyIphFeatureUsed(
        autofill::AutofillClient::IphFeature::kAutofillAi);
  }

  return RespondNow(ArgumentList(
      api::autofill_private::SetAutofillAiOptInStatus::Results::Create(
          /*success=*/true)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetWalletablePassDetectionOptInStatusFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetWalletablePassDetectionOptInStatusFunction::Run() {
  return RespondNow(WithArguments(wallet::GetWalletablePassDetectionOptInStatus(
      autofill_client()->GetPrefs(), autofill_client()->GetIdentityManager())));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetWalletablePassDetectionOptInStatusFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetWalletablePassDetectionOptInStatusFunction::Run() {
  std::optional<autofill_private::SetWalletablePassDetectionOptInStatus::Params>
      params = autofill_private::SetWalletablePassDetectionOptInStatus::Params::
          Create(args());
  EXTENSION_FUNCTION_VALIDATE(params);

  const bool success = wallet::SetWalletablePassDetectionOptInStatus(
      autofill_client()->GetPrefs(), autofill_client()->GetIdentityManager(),
      wallet::GeoIpCountryCode(
          autofill_client()->GetVariationConfigCountryCode().value()),
      params->opted_in);
  return RespondNow(WithArguments(success));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction

AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction::
    AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction() = default;

AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction::
    ~AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction() = default;

ExtensionFunction::ResponseAction
AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction::Run() {
  if (!autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
          autofill_client()->GetPrefs())) {
    return RespondNow(WithArguments(true));
  }

  autofill::ContentAutofillClient* client = autofill_client();
  if (!client) {
    return RespondNow(
        Error(base::StrCat({"Authenticate user before viewing entity data - ",
                            kErrorAutofillClientUnavailable})));
  }

  authenticator_ = client->GetDeviceAuthenticator();
  if (!authenticator_ ||
      !authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    return RespondNow(WithArguments(true));
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  message = l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_VIEWING_REAUTH);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  authenticator_->AuthenticateWithMessage(
      message,
      base::BindOnce(
          &AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction::
              OnReauthCompleted,
          this));
  return RespondLater();
}

void AutofillPrivateAuthenticateUserBeforeViewingEntityDataFunction::
    OnReauthCompleted(bool auth_succeeded) {
  authenticator_.reset();

  Respond(WithArguments(auth_succeeded));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateToggleAutofillAiReauthRequirementFunction

AutofillPrivateToggleAutofillAiReauthRequirementFunction::
    AutofillPrivateToggleAutofillAiReauthRequirementFunction() = default;

AutofillPrivateToggleAutofillAiReauthRequirementFunction::
    ~AutofillPrivateToggleAutofillAiReauthRequirementFunction() = default;

ExtensionFunction::ResponseAction
AutofillPrivateToggleAutofillAiReauthRequirementFunction::Run() {
  if (!base::FeatureList::IsEnabled(
          autofill::features::kAutofillAiReauthRequired)) {
    return RespondNow(NoArguments());
  }

  autofill::ContentAutofillClient* client = autofill_client();
  if (!client) {
    return RespondNow(
        Error(base::StrCat({"Toggle autofill AI reauth requirement - ",
                            kErrorAutofillClientUnavailable})));
  }

  authenticator_ = client->GetDeviceAuthenticator();
  if (!authenticator_ ||
      !authenticator_->CanAuthenticateWithBiometricOrScreenLock()) {
    return RespondNow(NoArguments());
  }

  std::u16string message;
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  message =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_AI_UPDATE_REAUTH_REQUIREMENT);
#endif  // BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN) || BUILDFLAG(IS_CHROMEOS)
  authenticator_->AuthenticateWithMessage(
      message,
      base::BindOnce(&AutofillPrivateToggleAutofillAiReauthRequirementFunction::
                         OnReauthCompleted,
                     this));
  return RespondLater();
}

void AutofillPrivateToggleAutofillAiReauthRequirementFunction::
    OnReauthCompleted(bool auth_succeeded) {
  if (auth_succeeded) {
    const bool new_val =
        !autofill::prefs::IsAutofillAiReauthBeforeFillingEnabled(
            autofill_client()->GetPrefs());
    autofill::prefs::SetAutofillAiReauthBeforeFillingEnabled(
        autofill_client()->GetPrefs(), new_val);
  }
  Respond(NoArguments());
}

}  // namespace extensions
