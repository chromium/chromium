// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_private_api.h"

#include <stddef.h>

#include <utility>

#include "base/guid.h"
#include "base/metrics/user_metrics.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/browser_autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_flow.h"
#include "components/autofill/core/browser/payments/virtual_card_enrollment_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "components/signin/public/identity_manager/account_info.h"
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

namespace {

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorDataUnavailable[] = "Autofill data unavailable.";

// Constant to assign a user-verified verification status to the autofill
// profile.
constexpr auto kUserVerified = autofill::VerificationStatus::kUserVerified;

// Dictionary keys used for serializing AddressUiComponent. Those values
// are used as keys in JavaScript code and shouldn't be modified.
constexpr char kFieldTypeKey[] = "field";
constexpr char kFieldLengthKey[] = "isLongField";
constexpr char kFieldNameKey[] = "fieldName";
constexpr char kFieldRequired[] = "isRequired";

// Field names for the address components.
constexpr char kFullNameField[] = "FULL_NAME";
constexpr char kCompanyNameField[] = "COMPANY_NAME";
constexpr char kAddressLineField[] = "ADDRESS_LINES";
constexpr char kDependentLocalityField[] = "ADDRESS_LEVEL_3";
constexpr char kCityField[] = "ADDRESS_LEVEL_2";
constexpr char kStateField[] = "ADDRESS_LEVEL_1";
constexpr char kPostalCodeField[] = "POSTAL_CODE";
constexpr char kSortingCodeField[] = "SORTING_CODE";
constexpr char kCountryField[] = "COUNTY_CODE";

// Converts an autofill::ServerFieldType to string format. Used in serilization
// of field type info to be used in JavaScript code, and hence those values
// shouldn't be modified.
const char* GetStringFromAddressField(i18n::addressinput::AddressField type) {
  switch (type) {
    case i18n::addressinput::RECIPIENT:
      return kFullNameField;
    case i18n::addressinput::ORGANIZATION:
      return kCompanyNameField;
    case i18n::addressinput::STREET_ADDRESS:
      return kAddressLineField;
    case i18n::addressinput::DEPENDENT_LOCALITY:
      return kDependentLocalityField;
    case i18n::addressinput::LOCALITY:
      return kCityField;
    case i18n::addressinput::ADMIN_AREA:
      return kStateField;
    case i18n::addressinput::POSTAL_CODE:
      return kPostalCodeField;
    case i18n::addressinput::SORTING_CODE:
      return kSortingCodeField;
    case i18n::addressinput::COUNTRY:
      return kCountryField;
    default:
      NOTREACHED();
      return "";
  }
}

// Serializes the AddressUiComponent a map from string to base::Value().
base::Value::Dict AddressUiComponentAsValueMap(
    const autofill::ExtendedAddressUiComponent& address_ui_component) {
  base::Value::Dict info;
  info.Set(kFieldNameKey, address_ui_component.name);
  info.Set(kFieldTypeKey,
           GetStringFromAddressField(address_ui_component.field));
  info.Set(kFieldLengthKey,
           address_ui_component.length_hint ==
               i18n::addressinput::AddressUiComponent::HINT_LONG);
  info.Set(kFieldRequired, address_ui_component.is_required);
  return info;
}

// Searches the |list| for the value at |index|.  If this value is present in
// any of the rest of the list, then the item (at |index|) is removed. The
// comparison of phone number values is done on normalized versions of the phone
// number values.
void RemoveDuplicatePhoneNumberAtIndex(size_t index,
                                       const std::string& country_code,
                                       base::Value::List& list) {
  if (list.size() <= index) {
    NOTREACHED() << "List should have a value at index " << index;
    return;
  }
  const std::string& new_value = list[index].GetString();

  bool is_duplicate = false;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < list.size() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    const std::string& existing_value = list[i].GetString();
    is_duplicate = autofill::i18n::PhoneNumbersMatch(
        base::UTF8ToUTF16(new_value), base::UTF8ToUTF16(existing_value),
        country_code, app_locale);
  }

  if (is_duplicate)
    list.erase(list.begin() + index);
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

autofill::AutofillProfile CreateNewAutofillProfile() {
  if (!base::FeatureList::IsEnabled(
          autofill::features::test::
              kAutofillCreateAccountProfilesFromSettings)) {
    return autofill::AutofillProfile(base::GenerateGUID(), kSettingsOrigin);
  }
  autofill::AutofillProfile profile(
      base::GenerateGUID(), kSettingsOrigin,
      autofill::AutofillProfile::Source::kAccount);
  profile.set_initial_creator_id(
      autofill::AutofillProfile::kInitialCreatorOrModifierChrome);
  profile.set_last_modifier_id(
      autofill::AutofillProfile::kInitialCreatorOrModifierChrome);
  return profile;
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
      existing_profile ? *existing_profile : CreateNewAutofillProfile();

  if (address->full_names) {
    std::string full_name;
    if (!address->full_names->empty())
      full_name = address->full_names->at(0);
    profile.SetInfoWithVerificationStatus(
        autofill::AutofillType(autofill::NAME_FULL),
        base::UTF8ToUTF16(full_name), g_browser_process->GetApplicationLocale(),
        kUserVerified);
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

  if (address->phone_numbers) {
    std::string phone;
    if (!address->phone_numbers->empty())
      phone = address->phone_numbers->at(0);
    profile.SetRawInfoWithVerificationStatus(autofill::PHONE_HOME_WHOLE_NUMBER,
                                             base::UTF8ToUTF16(phone),
                                             kUserVerified);
  }

  if (address->email_addresses) {
    std::string email;
    if (!address->email_addresses->empty())
      email = address->email_addresses->at(0);
    profile.SetRawInfoWithVerificationStatus(
        autofill::EMAIL_ADDRESS, base::UTF8ToUTF16(email), kUserVerified);
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (use_existing_profile) {
    profile.set_origin(kSettingsOrigin);
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

  std::vector<std::vector<autofill::ExtendedAddressUiComponent>> lines;
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
    for (const autofill::ExtendedAddressUiComponent& component : line) {
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
      existing_card
          ? *existing_card
          : autofill::CreditCard(base::GenerateGUID(), kSettingsOrigin);

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
// AutofillPrivateValidatePhoneNumbersFunction

ExtensionFunction::ResponseAction
AutofillPrivateValidatePhoneNumbersFunction::Run() {
  absl::optional<api::autofill_private::ValidatePhoneNumbers::Params>
      parameters =
          api::autofill_private::ValidatePhoneNumbers::Params::Create(args());
  EXTENSION_FUNCTION_VALIDATE(parameters);

  api::autofill_private::ValidatePhoneParams& params = parameters->params;

  // Extract the phone numbers into a base::Value::List.
  base::Value::List phone_numbers;
  for (auto phone_number : params.phone_numbers) {
    phone_numbers.Append(phone_number);
  }

  RemoveDuplicatePhoneNumberAtIndex(params.index_of_new_number,
                                    params.country_code, phone_numbers);

  return RespondNow(WithArguments(std::move(phone_numbers)));
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
  if (!autofill_manager || !autofill_manager->client())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the FormDataImporter from AutofillClient. FormDataImporter owns
  // LocalCardMigrationManager.
  autofill::FormDataImporter* form_data_importer =
      autofill_manager->client()->GetFormDataImporter();
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
      existing_iban ? *existing_iban : autofill::IBAN(base::GenerateGUID());

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
  if (!autofill_manager || !autofill_manager->client() ||
      !autofill_manager->client()->GetFormDataImporter() ||
      !autofill_manager->client()
           ->GetFormDataImporter()
           ->GetVirtualCardEnrollmentManager()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          ->GetFormDataImporter()
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
  if (!autofill_manager || !autofill_manager->client() ||
      !autofill_manager->client()->GetFormDataImporter() ||
      !autofill_manager->client()
           ->GetFormDataImporter()
           ->GetVirtualCardEnrollmentManager()) {
    return RespondNow(Error(kErrorDataUnavailable));
  }

  autofill::VirtualCardEnrollmentManager* virtual_card_enrollment_manager =
      autofill_manager->client()
          ->GetFormDataImporter()
          ->GetVirtualCardEnrollmentManager();

  virtual_card_enrollment_manager->Unenroll(
      card->instrument_id(),
      /*virtual_card_enrollment_update_response_callback=*/absl::nullopt);
  return RespondNow(NoArguments());
}

}  // namespace extensions
