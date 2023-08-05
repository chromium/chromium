// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>

#include <utility>

#include "base/functional/bind.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/uuid.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_client.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_experiments.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/metrics/payments/mandatory_reauth_metrics.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/mandatory_reauth_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/strings/grit/components_chromium_strings.h"
#include "components/strings/grit/components_google_chrome_strings.h"
#include "components/strings/grit/components_strings.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
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
  info.Set(kFieldTypeKey, FieldTypeToStringPiece(address_ui_component.field));
  info.Set(kFieldLengthKey,
           address_ui_component.length_hint ==
               autofill::AutofillAddressUIComponent::HINT_LONG);
  info.Set(kFieldRequired, address_ui_component.is_required);
  return info;
}

autofill::AutofillManager* GetAutofillManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents)
          ->DriverForFrame(web_contents->GetPrimaryMainFrame());
  if (!autofill_driver)
    return nullptr;
  return autofill_driver->autofill_manager();
}

autofill::AutofillProfile CreateNewAutofillProfile(
    autofill::PersonalDataManager* personal_data,
    absl::optional<base::StringPiece> country_code) {
  autofill::AutofillProfile::Source source =
      personal_data->IsEligibleForAddressAccountStorage()
          ? autofill::AutofillProfile::Source::kAccount
          : autofill::AutofillProfile::Source::kLocalOrSyncable;

  if (base::FeatureList::IsEnabled(
          autofill::features::test::
              kAutofillCreateAccountProfilesFromSettings)) {
    // Note: overriding address profile source only if test feature is enabled.
    source = autofill::AutofillProfile::Source::kAccount;
  }
  if (country_code && !personal_data->IsCountryEligibleForAccountStorage(
                          country_code.value())) {
    // Note: addresses from unsupported countries can't be saved in account.
    // TODO(crbug.com/1432505): remove temporary unsupported countries
    // filtering.
    source = autofill::AutofillProfile::Source::kLocalOrSyncable;
  }
  return autofill::AutofillProfile(source);
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAccountInfoFunction

ExtensionFunction::ResponseAction AutofillPrivateGetAccountInfoFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

  absl::optional<api::autofill_private::AccountInfo> account_info =
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
  absl::optional<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::AddressEntry* address = &parameters->address;

  // If a profile guid is specified, get a copy of the profile identified by it.
  // Otherwise create a new one.
  std::string guid = address->guid ? *address->guid : "";
  const bool use_existing_profile = !guid.empty();
  const autofill::AutofillProfile* existing_profile = nullptr;
  if (use_existing_profile) {
    existing_profile = personal_data->GetProfileByGUID(guid);
    if (!existing_profile)
      return RespondNow(Error(kErrorDataUnavailable));
  }
  autofill::AutofillProfile profile =
      existing_profile
          ? *existing_profile
          : CreateNewAutofillProfile(personal_data, address->country_code);

  if (address->full_name) {
    profile.SetInfoWithVerificationStatus(
        autofill::AutofillType(autofill::NAME_FULL),
        base::UTF8ToUTF16(*address->full_name),
        g_browser_process->GetApplicationLocale(), kUserVerified);
  }

  if (address->honorific) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::NAME_HONORIFIC_PREFIX, base::UTF8ToUTF16(*address->honorific),
        kUserVerified);
  }

  if (address->company_name) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::COMPANY_NAME, base::UTF8ToUTF16(*address->company_name),
        kUserVerified);
  }

  if (address->address_lines) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_STREET_ADDRESS,
        base::UTF8ToUTF16(*address->address_lines), kUserVerified);
  }

  if (address->address_level1) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_STATE,
        base::UTF8ToUTF16(*address->address_level1), kUserVerified);
  }

  if (address->address_level2) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_CITY,
        base::UTF8ToUTF16(*address->address_level2), kUserVerified);
  }

  if (address->address_level3) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
        base::UTF8ToUTF16(*address->address_level3), kUserVerified);
  }

  if (address->postal_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_ZIP, base::UTF8ToUTF16(*address->postal_code),
        kUserVerified);
  }

  if (address->sorting_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_SORTING_CODE,
        base::UTF8ToUTF16(*address->sorting_code), kUserVerified);
  }

  if (address->country_code) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::ADDRESS_HOME_COUNTRY,
        base::UTF8ToUTF16(*address->country_code), kUserVerified);
  }

  if (address->phone_number) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::PHONE_HOME_WHOLE_NUMBER,
        base::UTF8ToUTF16(*address->phone_number), kUserVerified);
  }

  if (address->email_address) {
    profile.SetRawInfoWithVerificationStatus(
        autofill::EMAIL_ADDRESS, base::UTF8ToUTF16(*address->email_address),
        kUserVerified);
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (use_existing_profile) {
    personal_data->UpdateProfile(profile);
  } else {
    profile.FinalizeAfterImport();
    personal_data->AddProfile(profile);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  // Return an empty list if data is not loaded.
  if (!(personal_data && personal_data->IsDataLoaded())) {
    autofill_util::CountryEntryList empty_list;
    return RespondNow(ArgumentList(
        api::autofill_private::GetCountryList::Results::Create(empty_list)));
  }

  autofill_util::CountryEntryList country_list =
      autofill_util::GenerateCountryList(*personal_data);

  return RespondNow(ArgumentList(
      api::autofill_private::GetCountryList::Results::Create(country_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressComponentsFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetAddressComponentsFunction::Run() {
  absl::optional<api::autofill_private::GetAddressComponents::Params>
      parameters =
          api::autofill_private::GetAddressComponents::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  std::vector<std::vector<autofill::AutofillAddressUIComponent>> lines;
  std::string language_code;

  autofill::GetAddressComponents(
      parameters->country_code, g_browser_process->GetApplicationLocale(),
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
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*personal_data);
  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  absl::optional<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::CreditCardEntry* card = &parameters->card;

  // If a card guid is specified, get a copy of the card identified by it.
  // Otherwise create a new one.
  std::string guid = card->guid ? *card->guid : "";
  const bool use_existing_card = !guid.empty();
  const autofill::CreditCard* existing_card = nullptr;
  if (use_existing_card) {
    existing_card = personal_data->GetCreditCardByGUID(guid);
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

  if (use_existing_card) {
    // Only updates when the card info changes.
    if (existing_card && existing_card->Compare(credit_card) == 0)
      return RespondNow(NoArguments());

    // Record when nickname is updated.
    if (credit_card.HasNonEmptyValidNickname() &&
        existing_card->nickname() != credit_card.nickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsEditedWithNickname"));
    }

    personal_data->UpdateCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsEdited"));
  } else {
    personal_data->AddCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsAdded"));
    if (credit_card.HasNonEmptyValidNickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillCreditCardsAddedWithNickname"));
    }
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntryFunction

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  absl::optional<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  if (personal_data->GetIBANByGUID(parameters->guid)) {
    base::RecordAction(base::UserMetricsAction("AutofillIbanDeleted"));
  }

  personal_data->RemoveByGUID(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMaskCreditCardFunction

ExtensionFunction::ResponseAction AutofillPrivateMaskCreditCardFunction::Run() {
  absl::optional<api::autofill_private::MaskCreditCard::Params> parameters =
      api::autofill_private::MaskCreditCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->ResetFullServerCard(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

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
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the BrowserAutofillManager from the web contents.
  // BrowserAutofillManager has a pointer to its AutofillClient which owns
  // FormDataImporter.
  autofill::AutofillManager* autofill_manager =
      GetAutofillManager(GetSenderWebContents());
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
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->LogServerCardLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction

ExtensionFunction::ResponseAction
AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction::Run() {
  // Getting CreditCardAccessManager from WebContents.
  autofill::AutofillManager* autofill_manager =
      GetAutofillManager(GetSenderWebContents());
  if (!autofill_manager)
    return RespondNow(Error(kErrorDataUnavailable));
  autofill::CreditCardAccessManager* credit_card_access_manager =
      autofill_manager->GetCreditCardAccessManager();
  if (!credit_card_access_manager)
    return RespondNow(Error(kErrorDataUnavailable));

  absl::optional<
      api::autofill_private::SetCreditCardFIDOAuthEnabledState::Params>
      parameters = api::autofill_private::SetCreditCardFIDOAuthEnabledState::
          Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  credit_card_access_manager->OnSettingsPageFIDOAuthToggled(
      parameters->enabled);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveIbanFunction

ExtensionFunction::ResponseAction AutofillPrivateSaveIbanFunction::Run() {
  absl::optional<api::autofill_private::SaveIban::Params> parameters =
      api::autofill_private::SaveIban::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  api::autofill_private::IbanEntry* iban_entry = &parameters->iban;
  DCHECK(iban_entry->value);

  // The IBAN guid is specified if the user tries to update an existing IBAN via
  // the Chrome payment settings page. Otherwise, leaving it blank creates a new
  // IBAN.
  std::string guid = iban_entry->guid ? *iban_entry->guid : "";
  const autofill::IBAN* existing_iban = nullptr;
  if (!guid.empty()) {
    existing_iban = personal_data->GetIBANByGUID(guid);
    if (!existing_iban)
      return RespondNow(Error(kErrorDataUnavailable));
  }
  autofill::IBAN iban =
      existing_iban
          ? *existing_iban
          : autofill::IBAN(base::Uuid::GenerateRandomV4().AsLowercaseString());

  iban.SetRawInfo(autofill::IBAN_VALUE, base::UTF8ToUTF16(*iban_entry->value));

  if (iban_entry->nickname)
    iban.set_nickname(base::UTF8ToUTF16(*iban_entry->nickname));

  if (guid.empty()) {
    personal_data->AddIBAN(iban);
    base::RecordAction(base::UserMetricsAction("AutofillIbanAdded"));
    if (!iban.nickname().empty()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillIbanAddedWithNickname"));
    }
    return RespondNow(NoArguments());
  }

  if (existing_iban->Compare(iban) != 0) {
    personal_data->UpdateIBAN(iban);
    base::RecordAction(base::UserMetricsAction("AutofillIbanEdited"));
    // Record when nickname is updated.
    if (existing_iban->nickname() != iban.nickname()) {
      base::RecordAction(
          base::UserMetricsAction("AutofillIbanEditedWithNickname"));
    }
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetIbanListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetIbanListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::IbanEntryList iban_list =
      autofill_util::GenerateIbanList(*personal_data);
  return RespondNow(ArgumentList(
      api::autofill_private::GetIbanList::Results::Create(iban_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateIsValidIbanFunction

ExtensionFunction::ResponseAction AutofillPrivateIsValidIbanFunction::Run() {
  absl::optional<api::autofill_private::IsValidIban::Params> parameters =
      api::autofill_private::IsValidIban::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);
  return RespondNow(WithArguments(
      autofill::IBAN::IsValid(base::UTF8ToUTF16(parameters->iban_value))));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetUpiIdListFunction

ExtensionFunction::ResponseAction AutofillPrivateGetUpiIdListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  DCHECK(personal_data && personal_data->IsDataLoaded());

  return RespondNow(
      ArgumentList(api::autofill_private::GetUpiIdList::Results::Create(
          personal_data->GetUpiIds())));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAddVirtualCardFunction

ExtensionFunction::ResponseAction AutofillPrivateAddVirtualCardFunction::Run() {
  absl::optional<api::autofill_private::AddVirtualCard::Params> parameters =
      api::autofill_private::AddVirtualCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  // Get the PersonalDataManager to retrieve the card based on the id.
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::CreditCard* card =
      personal_data_manager->GetCreditCardByServerId(parameters->card_id);
  if (!card)
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::AutofillManager* autofill_manager =
      GetAutofillManager(GetSenderWebContents());
  if (!autofill_manager || !autofill_manager->client().GetFormDataImporter() ||
      !autofill_manager->client()
           .GetFormDataImporter()
           ->GetVirtualCardEnrollmentManager()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          .GetFormDataImporter()
          ->GetVirtualCardEnrollmentManager();

  virtual_card_enrollment_manager->InitVirtualCardEnroll(
      *card, autofill::VirtualCardEnrollmentSource::kSettingsPage);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveVirtualCardFunction

ExtensionFunction::ResponseAction
AutofillPrivateRemoveVirtualCardFunction::Run() {
  absl::optional<api::autofill_private::RemoveVirtualCard::Params> parameters =
      api::autofill_private::RemoveVirtualCard::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  // Get the PersonalDataManager to retrieve the card based on the id.
  autofill::PersonalDataManager* personal_data_manager =
      autofill::PersonalDataManagerFactory::GetForProfile(
          Profile::FromBrowserContext(browser_context()));
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::CreditCard* card =
      personal_data_manager->GetCreditCardByServerId(parameters->card_id);
  if (!card)
    return RespondNow(Error(kErrorDataUnavailable));

  autofill::AutofillManager* autofill_manager =
      GetAutofillManager(GetSenderWebContents());
  if (!autofill_manager || !autofill_manager->client().GetFormDataImporter() ||
      !autofill_manager->client()
           .GetFormDataImporter()
           ->GetVirtualCardEnrollmentManager()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          .GetFormDataImporter()
          ->GetVirtualCardEnrollmentManager();

  virtual_card_enrollment_manager->Unenroll(
      card->instrument_id(),
      /*virtual_card_enrollment_update_response_callback=*/absl::nullopt);
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction

ExtensionFunction::ResponseAction
AutofillPrivateAuthenticateUserAndFlipMandatoryAuthToggleFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // If `client` is not available, then don't do anything.
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDeviceAuthUnavailable));
  }

  const std::u16string message =
      l10n_util::GetStringUTF16(IDS_PAYMENTS_AUTOFILL_MANDATORY_REAUTH_PROMPT);

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
      !personal_data_manager->IsPaymentMethodsMandatoryReauthEnabled(),
      MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
  client->GetOrCreatePaymentsMandatoryReauthManager()->AuthenticateWithMessage(
      message,
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
  // This function is not called in incognito mode and therefore a
  // PersonalDataManager should always exist.
  CHECK(personal_data_manager);

  // `opt_in` bool denotes whether the user is trying to opt in or out of the
  // mandatory reauth feature. If the mandatory reauth toggle on the settings is
  // currently enabled, then the `opt_in` bool will be false because the user is
  // opting-out, otherwise the `opt_in` bool will be true.
  const bool opt_in =
      !personal_data_manager->IsPaymentMethodsMandatoryReauthEnabled();
  LogMandatoryReauthOptInOrOutUpdateEvent(
      MandatoryReauthOptInOrOutSource::kSettingsPage, opt_in,
      reauth_succeeded ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
                       : MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
  if (reauth_succeeded) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthSuccessfulForMandatoryAuthToggle"));
    personal_data_manager->SetPaymentMethodsMandatoryReauthEnabled(opt_in);
  }
#endif
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateAuthenticateUserToEditLocalCardFunction

ExtensionFunction::ResponseAction
AutofillPrivateAuthenticateUserToEditLocalCardFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  // If `client` is not available, then don't do anything.
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (!client) {
    return RespondNow(Error(kErrorDeviceAuthUnavailable));
  }

  // If `personal_data_manager` is not available, then don't do anything.
  autofill::PersonalDataManager* personal_data_manager =
      client->GetPersonalDataManager();
  if (!personal_data_manager || !personal_data_manager->IsDataLoaded()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }
  if (personal_data_manager->IsPaymentMethodsMandatoryReauthEnabled()) {
    const std::u16string message = l10n_util::GetStringUTF16(
        IDS_PAYMENTS_AUTOFILL_EDIT_CARD_MANDATORY_REAUTH_PROMPT);

    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthTriggeredToShowEditLocalCardDialog"));
    LogMandatoryReauthSettingsPageEditCardEvent(
        MandatoryReauthAuthenticationFlowEvent::kFlowStarted);
    // Based on the result of the auth, we will be asynchronously returning if
    // the user can edit the local card.
    client->GetOrCreatePaymentsMandatoryReauthManager()
        ->AuthenticateWithMessage(
            message,
            base::BindOnce(
                &AutofillPrivateAuthenticateUserToEditLocalCardFunction::
                    CanShowEditDialogForLocalCard,
                this));

    // Due to async nature of AuthenticateWithMessage() on mandatory re-auth
    // manager we use the below check to make sure we have a `Respond` captured.
    // If we didn't have this check, then we would show the edit card dialog box
    // even before the user successfully completes the auth.
    return did_respond() ? AlreadyResponded() : RespondLater();
  }
#endif
  return RespondNow(WithArguments(true));
}

// Return the auth result for showing the edit card dialog for local card. We
// also log whether the auth was successful or not.
void AutofillPrivateAuthenticateUserToEditLocalCardFunction::
    CanShowEditDialogForLocalCard(bool can_show) {
  LogMandatoryReauthSettingsPageEditCardEvent(
      can_show ? MandatoryReauthAuthenticationFlowEvent::kFlowSucceeded
               : MandatoryReauthAuthenticationFlowEvent::kFlowFailed);
  if (can_show) {
    base::RecordAction(base::UserMetricsAction(
        "PaymentsUserAuthSuccessfulToShowEditLocalCardDialog"));
  }
  Respond(WithArguments(can_show));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateCheckIfDeviceAuthAvailableFunction

ExtensionFunction::ResponseAction
AutofillPrivateCheckIfDeviceAuthAvailableFunction::Run() {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  autofill::ContentAutofillClient* client =
      autofill::ContentAutofillClient::FromWebContents(GetSenderWebContents());
  if (client) {
    return RespondNow(WithArguments(
        autofill::IsDeviceAuthAvailable(client->GetDeviceAuthenticator())));
  }
#endif  // BUILDFLAG (IS_MAC) || BUILDFLAG(IS_WIN)
  return RespondNow(Error(kErrorDeviceAuthUnavailable));
}

}  // namespace extensions
