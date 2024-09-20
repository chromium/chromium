// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>

#include <algorithm>
#include <optional>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/user_annotations/user_annotations_service_factory.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/core/browser/address_data_manager.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/address_save_metrics.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/credit_card_access_manager.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/payments_autofill_client.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/payments_data_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_branded_strings.h"
#include "components/strings/grit/components_strings.h"
#include "components/user_annotations/user_annotations_service.h"
#include "components/user_annotations/user_annotations_types.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;
namespace addressinput = i18n::addressinput;

using autofill::autofill_metrics::LogMandatoryReauthOptInOrOutUpdateEvent;
using autofill::autofill_metrics::LogMandatoryReauthSettingsPageEditCardEvent;
using autofill::autofill_metrics::MandatoryReauthAuthenticationFlowEvent;
using autofill::autofill_metrics::MandatoryReauthOptInOrOutSource;

namespace {

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorCardDataUnavailable[] = "Credit card data unavailable";
static const char kErrorDataUnavailable[] = "Autofill data unavailable.";
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
base::Value::Dict AddressUiComponentAsValueMap(
    const autofill::AutofillAddressUIComponent& address_ui_component) {
  base::Value::Dict info;
  info.Set(kFieldNameKey, address_ui_component.name);
  info.Set(kFieldTypeKey, FieldTypeToStringView(address_ui_component.field));
  info.Set(kFieldLengthKey,
           address_ui_component.length_hint ==
               autofill::AutofillAddressUIComponent::HINT_LONG);
  info.Set(kFieldRequired, address_ui_component.is_required);
  return info;
}

autofill::BrowserAutofillManager* GetBrowserAutofillManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriver::GetForRenderFrameHost(
          web_contents->GetPrimaryMainFrame());
  if (!autofill_driver)
    return nullptr;
  // This cast is safe, since `AutofillManager` is always a
  // `BrowserAutofillManager` apart from on WebView.
  return static_cast<autofill::BrowserAutofillManager*>(
      &autofill_driver->GetAutofillManager());
}

autofill::AutofillProfile CreateNewAutofillProfile(
    autofill::PersonalDataManager* personal_data,
    std::optional<std::string_view> country_code) {
  autofill::AutofillProfile::RecordType record_type =
      personal_data->address_data_manager().IsEligibleForAddressAccountStorage()
          ? autofill::AutofillProfile::RecordType::kAccount
          : autofill::AutofillProfile::RecordType::kLocalOrSyncable;
  if (country_code &&
      !personal_data->address_data_manager().IsCountryEligibleForAccountStorage(
          country_code.value())) {
    // Note: addresses from unsupported countries can't be saved in account.
    // TODO(crbug.com/40263955): remove temporary unsupported countries
    // filtering.
    record_type = autofill::AutofillProfile::RecordType::kLocalOrSyncable;
  }

  AddressCountryCode address_country_code =
      country_code.has_value()
          ? AddressCountryCode(std::string(*country_code))
          : autofill::i18n_model_definition::kLegacyHierarchyCountryCode;
  return autofill::AutofillProfile(record_type, address_country_code);
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAccountInfoFunction

ExtensionFunction::ResponseAction AutofillPrivateGetAccountInfoFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  std::optional<api::autofill_private::AccountInfo> account_info =
      autofill_util::GetAccountInfo(*personal_data);
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

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::AddressEntry* address = &parameters->address;

  // If a profile guid is specified, get a copy of the profile identified by it.
  // Otherwise create a new one.
  std::string guid = address->guid ? *address->guid : "";
  const bool use_existing_profile = !guid.empty();
  const autofill::AutofillProfile* existing_profile = nullptr;
  if (use_existing_profile) {
    existing_profile =
        personal_data->address_data_manager().GetProfileByGUID(guid);
    if (!existing_profile)
      return RespondNow(Error(kErrorDataUnavailable));
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
                       : CreateNewAutofillProfile(personal_data, country_code);

  // TODO(crbug.com/40266693): Fields not visible for the autofill profile's
  // country must be reset.
  for (const api::autofill_private::AddressField& field : address->fields) {
    if (field.type == autofill_private::FieldType::kNameFull) {
      profile.SetInfoWithVerificationStatus(
          autofill::NAME_FULL, base::UTF8ToUTF16(field.value),
          ExtensionsBrowserClient::Get()->GetApplicationLocale(),
          kUserVerified);
    } else {
      profile.SetRawInfoWithVerificationStatus(
          autofill::TypeNameToFieldType(autofill_private::ToString(field.type)),
          base::UTF8ToUTF16(field.value), kUserVerified);
    }
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (use_existing_profile) {
    personal_data->address_data_manager().UpdateProfile(profile);
  } else {
    profile.FinalizeAfterImport();
    personal_data->address_data_manager().AddProfile(profile);
    autofill::autofill_metrics::LogManuallyAddedAddress(
        autofill::autofill_metrics::AutofillManuallyAddedAddressSurface::
            kSettings);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  std::optional<api::autofill_private::GetCountryList::Params> parameters =
      api::autofill_private::GetCountryList::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();

  // Return an empty list if data is not loaded.
  if (!personal_data || !personal_data->IsDataLoaded()) {
    autofill_util::CountryEntryList empty_list;
    return RespondNow(ArgumentList(
        api::autofill_private::GetCountryList::Results::Create(empty_list)));
  }

  autofill_util::CountryEntryList country_list =
      autofill_util::GenerateCountryList(
          *personal_data, parameters->for_account_address_profile);

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
  base::Value::Dict address_components;
  base::Value::List rows;

  for (auto& line : lines) {
    base::Value::List row_values;
    for (const autofill::AutofillAddressUIComponent& component : line) {
      row_values.Append(AddressUiComponentAsValueMap(component));
    }
    base::Value::Dict row;
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
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*personal_data);
  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  std::optional<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }
  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));
  api::autofill_private::CreditCardEntry* card = &parameters->card;

  // If a card guid is specified, get a copy of the card identified by it.
  // Otherwise create a new one.
  std::string guid = card->guid ? *card->guid : "";
  const bool use_existing_card = !guid.empty();
  const autofill::CreditCard* existing_card = nullptr;
  if (use_existing_card) {
    existing_card =
        personal_data->payments_data_manager().GetCreditCardByGUID(guid);
    if (!existing_card)
      return RespondNow(Error(kErrorDataUnavailable));
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
    if (existing_card && existing_card->Compare(credit_card) == 0)
      return RespondNow(NoArguments());

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

    personal_data->payments_data_manager().UpdateCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsEdited"));
  } else {
    int current_card_count =
        personal_data->payments_data_manager().GetCreditCards().size();
    personal_data->payments_data_manager().AddCreditCard(credit_card);

    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsAdded"));
    base::UmaHistogramCounts100(
        "Autofill.PaymentMethods.SettingsPage."
        "StoredCreditCardCountBeforeCardAdded",
        current_card_count);

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
// AutofillPrivateRemoveEntryFunction

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  std::optional<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }
  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  if (personal_data->payments_data_manager().GetIbanByGUID(parameters->guid)) {
    base::RecordAction(base::UserMetricsAction("AutofillIbanDeleted"));
  } else if (const autofill::CreditCard* credit_card =
                 personal_data->payments_data_manager().GetCreditCardByGUID(
                     parameters->guid)) {
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
  personal_data->RemoveByGUID(parameters->guid);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill_util::CreditCardEntryList credit_card_list =
      autofill_util::GenerateCreditCardList(*personal_data);
  return RespondNow(
      ArgumentList(api::autofill_private::GetCreditCardList::Results::Create(
          credit_card_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMigrateCreditCardsFunction

ExtensionFunction::ResponseAction
AutofillPrivateMigrateCreditCardsFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the BrowserAutofillManager from the web contents.
  // BrowserAutofillManager has a pointer to its AutofillClient which owns
  // FormDataImporter.
  autofill::AutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // Get the FormDataImporter from AutofillClient. FormDataImporter owns
  // LocalCardMigrationManager.
  autofill::FormDataImporter* form_data_importer =
      autofill_manager->client().GetFormDataImporter();
  if (!form_data_importer)
    return RespondNow(Error(kErrorDataUnavailable));

  // Get local card migration manager from form data importer.
  autofill::LocalCardMigrationManager* local_card_migration_manager =
      form_data_importer->local_card_migration_manager();
  if (!local_card_migration_manager)
    return RespondNow(Error(kErrorDataUnavailable));

  // Since we already check the migration requirements on the settings page, we
  // don't check the migration requirements again.
  local_card_migration_manager->GetMigratableCreditCards();
  local_card_migration_manager->AttemptToOfferLocalCardMigration(
      /*is_from_settings_page=*/true);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLogServerCardLinkClickedFunction

ExtensionFunction::ResponseAction
AutofillPrivateLogServerCardLinkClickedFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->payments_data_manager().LogServerCardLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateLogServerIbanLinkClickedFunction

ExtensionFunction::ResponseAction
AutofillPrivateLogServerIbanLinkClickedFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();

  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  personal_data->payments_data_manager().LogServerIbanLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveIbanFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveIbanFunction::Run() {
  std::optional<api::autofill_private::SaveIban::Params> parameters =
      api::autofill_private::SaveIban::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::IbanEntry* iban_entry = &parameters->iban;
  CHECK(iban_entry->value);

  const autofill::Iban* existing_iban = nullptr;

  // The IBAN guid is specified if the user tries to update an existing IBAN via
  // the Chrome payment settings page.
  if (iban_entry->guid.has_value() && !iban_entry->guid->empty()) {
    existing_iban =
        personal_data->payments_data_manager().GetIbanByGUID(*iban_entry->guid);
    CHECK(existing_iban);
  }

  autofill::Iban iban_to_write =
      existing_iban ? *existing_iban : autofill::Iban();

  iban_to_write.SetRawInfo(autofill::IBAN_VALUE,
                           base::UTF8ToUTF16(*iban_entry->value));

  if (iban_entry->nickname) {
    iban_to_write.set_nickname(base::UTF8ToUTF16(*iban_entry->nickname));
  }

  // Add a new IBAN and return if this is not an update.
  if (!existing_iban) {
    personal_data->payments_data_manager().AddAsLocalIban(iban_to_write);
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
    personal_data->payments_data_manager().UpdateIban(iban_to_write);
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
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill_util::IbanEntryList iban_list =
      autofill_util::GenerateIbanList(*personal_data);
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

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  const autofill::CreditCard* card =
      personal_data_manager->payments_data_manager().GetCreditCardByServerId(
          parameters->card_id);
  if (!card)
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::BrowserAutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          .GetPaymentsAutofillClient()
          ->GetVirtualCardEnrollmentManager();

  virtual_card_enrollment_manager->InitVirtualCardEnroll(
      *card, autofill::VirtualCardEnrollmentSource::kSettingsPage);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveVirtualCardFunction

ExtensionFunction::ResponseAction
AutofillPrivateRemoveVirtualCardFunction::Run() {
  std::optional<api::autofill_private::RemoveVirtualCard::Params> parameters =
      api::autofill_private::RemoveVirtualCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  const autofill::CreditCard* card =
      personal_data_manager->payments_data_manager().GetCreditCardByServerId(
          parameters->card_id);
  if (!card)
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::BrowserAutofillManager* autofill_manager =
      GetBrowserAutofillManager(GetSenderWebContents());
  if (!autofill_manager) {
    return RespondNow(Error(kErrorDataUnavailable));
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
// AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction

ExtensionFunction::ResponseAction
AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // If `personal_data_manager` is not available or `IsDataLoaded` is false,
  // then don't do anything.
  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // We will be modifying the pref `kAutofillPaymentMethodsMandatoryReauth`
  // asynchronously. The pref value directly correlates to the mandatory auth
  // toggle.
  // We are also logging the start of the auth flow and
  // `!personal_data_manager->IsPaymentMethodsMandatoryReauthEnabled()` denotes
  // if the user is either opting in or out.
  base::RecordAction(base::UserMetricsAction(
      "PaymentsUserAuthTriggeredForMandatoryAuthToggle"));
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage,
      /*opt_in=*/
      !personal_data_manager->payments_data_manager()
           .IsPaymentMethodsMandatoryReauthEnabled(),
      MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
  client->GetPaymentsAutofillClient()
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
  return RespondNow(Error(kErrorDeviceAuthUnavailable));
#endif  // BUILDFLAG (IS_MAC) || BUILDFLAG(IS_WIN)
}

// Update the Mandatory auth toggle pref and log whether the auth was successful
// or not.
void AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::
    UpdateMandatoryAuthTogglePref(bool reauth_succeeded) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  content::WebContents* sender_web_contents = GetSenderWebContents();
  if (!sender_web_contents) {
    return;
  }
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(sender_web_contents);
  CHECK(client);
  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  CHECK(personal_data_manager);

  // `opt_in` bool denotes whether the user is trying to opt in or out of the
  // mandatory reauth feature. If the mandatory reauth toggle on the settings is
  // currently enabled, then the `opt_in` bool will be false because the user is
  // opting-out, otherwise the `opt_in` bool will be true.
  const bool opt_in = !personal_data_manager->payments_data_manager()
                           .IsPaymentMethodsMandatoryReauthEnabled();
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage, opt_in,
      reauth_succeeded ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
                       : MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
  if (reauth_succeeded) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthSuccessfulForMandatoryAuthToggle"));
    personal_data_manager->payments_data_manager()
        .SetPaymentMethodsMandatoryReauthEnabled(opt_in);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetLocalCardFunction

ExtensionFunction::ResponseAction AutofillPrivateGetLocalCardFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }
  if (personal_data_manager->payments_data_manager()
          .IsPaymentMethodsMandatoryReauthEnabled()) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
    LogMandatoryReauthSettingsPageEditCardEvent(
        MandatoryReauthAuthenticationFlowEvent::kFlowStarted);

#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
    // Based on the result of the auth, we will be asynchronously returning the
    // card if the user can edit the local card.
    client->GetPaymentsAutofillClient()
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
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  CHECK(client);
  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  CHECK(personal_data_manager);

  std::optional<autofill_private::GetLocalCard::Params> parameters =
      autofill_private::GetLocalCard::Params::Create(args());
  if (auto* card_from_guid =
          personal_data_manager->payments_data_manager().GetCreditCardByGUID(
              parameters->guid)) {
    return Respond(ArgumentList(autofill_private::GetLocalCard::Results::Create(
        autofill_util::CreditCardToCreditCardEntry(
            *card_from_guid, *personal_data_manager,
            /*mask_local_cards=*/false))));
  }
  return Respond(Error(kErrorCardDataUnavailable));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateCheckIfDeviceAuthAvailableFunction

ExtensionFunction::ResponseAction
AutofillPrivateCheckIfDeviceAuthAvailableFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (client) {
    return RespondNow(WithArguments(autofill::IsDeviceAuthAvailable(
        client->GetDeviceAuthenticator().get())));
  }
#endif  // BUILDFLAG (IS_MAC) || BUILDFLAG(IS_WIN)
  return RespondNow(Error(kErrorDeviceAuthUnavailable));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateBulkDeleteAllCvcsFunction

ExtensionFunction::ResponseAction
AutofillPrivateBulkDeleteAllCvcsFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  // Clear local and server CVCs from the webdata database. For server CVCs,
  // this will also clear them from the Chrome sync server and thus other
  // devices.
  personal_data->payments_data_manager().ClearLocalCvcs();
  personal_data->payments_data_manager().ClearServerCvcs();

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetAutofillSyncToggleEnabledFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetAutofillSyncToggleEnabledFunction::Run() {
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::PersonalDataManager* personal_data =
      client->GetPersonalDataManager();
  if (!personal_data || !personal_data->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  std::optional<api::autofill_private::SetAutofillSyncToggleEnabled::Params>
      parameters =
          api::autofill_private::SetAutofillSyncToggleEnabled::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  personal_data->address_data_manager().SetAutofillSelectableTypeEnabled(
      parameters->enabled);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetUserAnnotationsEntriesFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetUserAnnotationsEntriesFunction::Run() {
  Profile* profile =
      Profile::FromBrowserContext(GetSenderWebContents()->GetBrowserContext());
  user_annotations::UserAnnotationsService* user_annotations_service =
      profile ? UserAnnotationsServiceFactory::GetForProfile(profile) : nullptr;

  if (!user_annotations_service) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  user_annotations_service->RetrieveAllEntries(base::BindOnce(
      &AutofillPrivateGetUserAnnotationsEntriesFunction::OnEntriesRetrieved,
      this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutofillPrivateGetUserAnnotationsEntriesFunction::OnEntriesRetrieved(
    user_annotations::UserAnnotationsEntries response) {
  std::vector<autofill_private::UserAnnotationsEntry> result;
  result.reserve(response.size());
  for (optimization_guide::proto::UserAnnotationsEntry& entry : response) {
    result.emplace_back();
    result.back().entry_id = entry.entry_id();
    result.back().key = std::move(entry.key());
    result.back().value = std::move(entry.value());
  }
  Respond(ArgumentList(
      api::autofill_private::GetUserAnnotationsEntries::Results::Create(
          result)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateDeleteUserAnnotationsEntryFunction

ExtensionFunction::ResponseAction
AutofillPrivateDeleteUserAnnotationsEntryFunction::Run() {
  std::optional<api::autofill_private::DeleteUserAnnotationsEntry::Params>
      parameters =
          api::autofill_private::DeleteUserAnnotationsEntry::Params::Create(
              args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  Profile* profile =
      Profile::FromBrowserContext(GetSenderWebContents()->GetBrowserContext());
  user_annotations::UserAnnotationsService* user_annotations_service =
      profile ? UserAnnotationsServiceFactory::GetForProfile(profile) : nullptr;

  if (!user_annotations_service) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  user_annotations_service->RemoveEntry(
      parameters->entry_id,
      base::BindOnce(
          &AutofillPrivateDeleteUserAnnotationsEntryFunction::OnEntryDeleted,
          this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutofillPrivateDeleteUserAnnotationsEntryFunction::OnEntryDeleted() {
  Respond(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateDeleteAllUserAnnotationsEntriesFunction

ExtensionFunction::ResponseAction
AutofillPrivateDeleteAllUserAnnotationsEntriesFunction::Run() {
  Profile* profile =
      Profile::FromBrowserContext(GetSenderWebContents()->GetBrowserContext());
  user_annotations::UserAnnotationsService* user_annotations_service =
      profile ? UserAnnotationsServiceFactory::GetForProfile(profile) : nullptr;

  if (!user_annotations_service) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  user_annotations_service->RemoveAllEntries(
      base::BindOnce(&AutofillPrivateDeleteAllUserAnnotationsEntriesFunction::
                         OnAllEntriesDeleted,
                     this));

  return did_respond() ? AlreadyResponded() : RespondLater();
}

void AutofillPrivateDeleteAllUserAnnotationsEntriesFunction::
    OnAllEntriesDeleted() {
  Respond(NoArguments());
}

}  // namespace extensions
