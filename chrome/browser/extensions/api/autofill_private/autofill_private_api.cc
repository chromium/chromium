// Copyright 2015 The Chromium Authors. All rights reserved.
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
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/content/browser/content_autofill_driver.h"
#include "components/autofill/content/browser/content_autofill_driver_factory.h"
#include "components/autofill/core/browser/autofill_address_util.h"
#include "components/autofill/core/browser/autofill_manager.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/form_data_importer.h"
#include "components/autofill/core/browser/payments/local_card_migration_manager.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/common/autofill_features.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/extension_function_registry.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/address_ui_component.h"
#include "third_party/libaddressinput/src/cpp/include/libaddressinput/localization.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;
namespace addressinput = i18n::addressinput;

namespace {

static const char kSettingsOrigin[] = "Chrome settings";
static const char kErrorDataUnavailable[] = "Autofill data unavailable.";

// Searches the |list| for the value at |index|.  If this value is present in
// any of the rest of the list, then the item (at |index|) is removed. The
// comparison of phone number values is done on normalized versions of the phone
// number values.
void RemoveDuplicatePhoneNumberAtIndex(
    size_t index, const std::string& country_code, base::ListValue* list) {
  base::string16 new_value;
  if (!list->GetString(index, &new_value)) {
    NOTREACHED() << "List should have a value at index " << index;
    return;
  }

  bool is_duplicate = false;
  std::string app_locale = g_browser_process->GetApplicationLocale();
  for (size_t i = 0; i < list->GetSize() && !is_duplicate; ++i) {
    if (i == index)
      continue;

    base::string16 existing_value;
    if (!list->GetString(i, &existing_value)) {
      NOTREACHED() << "List should have a value at index " << i;
      continue;
    }
    is_duplicate = autofill::i18n::PhoneNumbersMatch(
        new_value, existing_value, country_code, app_locale);
  }

  if (is_duplicate)
    list->Remove(index, nullptr);
}

autofill::AutofillManager* GetAutofillManager(
    content::WebContents* web_contents) {
  if (!web_contents) {
    return nullptr;
  }
  autofill::ContentAutofillDriver* autofill_driver =
      autofill::ContentAutofillDriverFactory::FromWebContents(web_contents)
          ->DriverForFrame(web_contents->GetMainFrame());
  if (!autofill_driver)
    return nullptr;
  return autofill_driver->autofill_manager();
}

}  // namespace

namespace extensions {

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveAddressFunction

AutofillPrivateSaveAddressFunction::AutofillPrivateSaveAddressFunction()
    : chrome_details_(this) {}

AutofillPrivateSaveAddressFunction::~AutofillPrivateSaveAddressFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveAddressFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveAddress::Params> parameters =
      api::autofill_private::SaveAddress::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
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
          : autofill::AutofillProfile(base::GenerateGUID(), kSettingsOrigin);

  if (address->full_names) {
    std::string full_name;
    if (!address->full_names->empty())
      full_name = address->full_names->at(0);
    profile.SetInfo(autofill::AutofillType(autofill::NAME_FULL),
                    base::UTF8ToUTF16(full_name),
                    g_browser_process->GetApplicationLocale());
  }

  if (address->company_name) {
    profile.SetRawInfo(
        autofill::COMPANY_NAME,
        base::UTF8ToUTF16(*address->company_name));
  }

  if (address->address_lines) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_STREET_ADDRESS,
        base::UTF8ToUTF16(*address->address_lines));
  }

  if (address->address_level1) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_STATE,
        base::UTF8ToUTF16(*address->address_level1));
  }

  if (address->address_level2) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_CITY,
        base::UTF8ToUTF16(*address->address_level2));
  }

  if (address->address_level3) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_DEPENDENT_LOCALITY,
        base::UTF8ToUTF16(*address->address_level3));
  }

  if (address->postal_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_ZIP,
        base::UTF8ToUTF16(*address->postal_code));
  }

  if (address->sorting_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_SORTING_CODE,
        base::UTF8ToUTF16(*address->sorting_code));
  }

  if (address->country_code) {
    profile.SetRawInfo(
        autofill::ADDRESS_HOME_COUNTRY,
        base::UTF8ToUTF16(*address->country_code));
  }

  if (address->phone_numbers) {
    std::string phone;
    if (!address->phone_numbers->empty())
      phone = address->phone_numbers->at(0);
    profile.SetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER,
                       base::UTF8ToUTF16(phone));
  }

  if (address->email_addresses) {
    std::string email;
    if (!address->email_addresses->empty())
      email = address->email_addresses->at(0);
    profile.SetRawInfo(autofill::EMAIL_ADDRESS, base::UTF8ToUTF16(email));
  }

  if (address->language_code)
    profile.set_language_code(*address->language_code);

  if (use_existing_profile) {
    profile.set_origin(kSettingsOrigin);
    personal_data->UpdateProfile(profile);
  } else {
    personal_data->AddProfile(profile);
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCountryListFunction

AutofillPrivateGetCountryListFunction::AutofillPrivateGetCountryListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetCountryListFunction::
    ~AutofillPrivateGetCountryListFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateGetCountryListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

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

AutofillPrivateGetAddressComponentsFunction::
    ~AutofillPrivateGetAddressComponentsFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateGetAddressComponentsFunction::Run() {
  std::unique_ptr<api::autofill_private::GetAddressComponents::Params>
      parameters =
          api::autofill_private::GetAddressComponents::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  auto components = std::make_unique<base::ListValue>();
  std::string language_code_;

  autofill::GetAddressComponents(parameters->country_code,
                                 g_browser_process->GetApplicationLocale(),
                                 components.get(), &language_code_);

  // Convert ListValue to AddressComponents
  base::Value address_components(base::Value::Type::DICTIONARY);
  base::Value rows(base::Value::Type::LIST);

  for (auto& component : components->GetList()) {
    base::Value row(base::Value::Type::DICTIONARY);
    row.SetKey("row", std::move(component));
    rows.Append(std::move(row));
  }

  address_components.SetKey("components", std::move(rows));
  address_components.SetKey("languageCode", base::Value(language_code_));

  return RespondNow(OneArgument(
      base::Value::ToUniquePtrValue(std::move(address_components))));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetAddressListFunction

AutofillPrivateGetAddressListFunction::AutofillPrivateGetAddressListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetAddressListFunction::
    ~AutofillPrivateGetAddressListFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateGetAddressListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::AddressEntryList address_list =
      autofill_util::GenerateAddressList(*personal_data);
  return RespondNow(ArgumentList(
      api::autofill_private::GetAddressList::Results::Create(address_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSaveCreditCardFunction

AutofillPrivateSaveCreditCardFunction::AutofillPrivateSaveCreditCardFunction()
    : chrome_details_(this) {}

AutofillPrivateSaveCreditCardFunction::
    ~AutofillPrivateSaveCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateSaveCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::SaveCreditCard::Params> parameters =
      api::autofill_private::SaveCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
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
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_NUMBER,
        base::UTF8ToUTF16(*card->card_number));
  }

  if (card->expiration_month) {
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_EXP_MONTH,
        base::UTF8ToUTF16(*card->expiration_month));
  }

  if (card->expiration_year) {
    credit_card.SetRawInfo(
        autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
        base::UTF8ToUTF16(*card->expiration_year));
  }

  if (use_existing_card) {
    personal_data->UpdateCreditCard(credit_card);
  } else {
    personal_data->AddCreditCard(credit_card);
    base::RecordAction(base::UserMetricsAction("AutofillCreditCardsAdded"));
  }

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateRemoveEntryFunction

AutofillPrivateRemoveEntryFunction::AutofillPrivateRemoveEntryFunction()
    : chrome_details_(this) {}

AutofillPrivateRemoveEntryFunction::~AutofillPrivateRemoveEntryFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateRemoveEntryFunction::Run() {
  std::unique_ptr<api::autofill_private::RemoveEntry::Params> parameters =
      api::autofill_private::RemoveEntry::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->RemoveByGUID(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateValidatePhoneNumbersFunction

AutofillPrivateValidatePhoneNumbersFunction::
    ~AutofillPrivateValidatePhoneNumbersFunction() {}

ExtensionFunction::ResponseAction
    AutofillPrivateValidatePhoneNumbersFunction::Run() {
  std::unique_ptr<api::autofill_private::ValidatePhoneNumbers::Params>
      parameters =
          api::autofill_private::ValidatePhoneNumbers::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  api::autofill_private::ValidatePhoneParams* params = &parameters->params;

  // Extract the phone numbers into a ListValue.
  std::unique_ptr<base::ListValue> phone_numbers(new base::ListValue);
  phone_numbers->AppendStrings(params->phone_numbers);

  RemoveDuplicatePhoneNumberAtIndex(params->index_of_new_number,
                                    params->country_code, phone_numbers.get());

  return RespondNow(OneArgument(std::move(phone_numbers)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMaskCreditCardFunction

AutofillPrivateMaskCreditCardFunction::AutofillPrivateMaskCreditCardFunction()
    : chrome_details_(this) {}

AutofillPrivateMaskCreditCardFunction::
    ~AutofillPrivateMaskCreditCardFunction() {}

ExtensionFunction::ResponseAction AutofillPrivateMaskCreditCardFunction::Run() {
  std::unique_ptr<api::autofill_private::MaskCreditCard::Params> parameters =
      api::autofill_private::MaskCreditCard::Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
      chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->ResetFullServerCard(parameters->guid);

  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateGetCreditCardListFunction

AutofillPrivateGetCreditCardListFunction::
    AutofillPrivateGetCreditCardListFunction()
    : chrome_details_(this) {}

AutofillPrivateGetCreditCardListFunction::
    ~AutofillPrivateGetCreditCardListFunction() {}

ExtensionFunction::ResponseAction
AutofillPrivateGetCreditCardListFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  DCHECK(personal_data && personal_data->IsDataLoaded());

  autofill_util::CreditCardEntryList credit_card_list =
      autofill_util::GenerateCreditCardList(*personal_data);
  return RespondNow(
      ArgumentList(api::autofill_private::GetCreditCardList::Results::Create(
          credit_card_list)));
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateMigrateCreditCardsFunction

AutofillPrivateMigrateCreditCardsFunction::
    AutofillPrivateMigrateCreditCardsFunction()
    : chrome_details_(this) {}

AutofillPrivateMigrateCreditCardsFunction::
    ~AutofillPrivateMigrateCreditCardsFunction() {}

ExtensionFunction::ResponseAction
AutofillPrivateMigrateCreditCardsFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());
  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  // Get the AutofillManager from the web contents. AutofillManager has a
  // pointer to its AutofillClient which owns FormDataImporter.
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

AutofillPrivateLogServerCardLinkClickedFunction::
    AutofillPrivateLogServerCardLinkClickedFunction()
    : chrome_details_(this) {}

AutofillPrivateLogServerCardLinkClickedFunction::
    ~AutofillPrivateLogServerCardLinkClickedFunction() {}

ExtensionFunction::ResponseAction
AutofillPrivateLogServerCardLinkClickedFunction::Run() {
  autofill::PersonalDataManager* personal_data =
      autofill::PersonalDataManagerFactory::GetForProfile(
          chrome_details_.GetProfile());

  if (!personal_data || !personal_data->IsDataLoaded())
    return RespondNow(Error(kErrorDataUnavailable));

  personal_data->LogServerCardLinkClicked();
  return RespondNow(NoArguments());
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction

AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction::
    AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction()
    : chrome_details_(this) {}

AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction::
    ~AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction() {}

ExtensionFunction::ResponseAction
AutofillPrivateSetCreditCardFIDOAuthEnabledStateFunction::Run() {
  // Getting CreditCardAccessManager from WebContents.
  autofill::AutofillManager* autofill_manager =
      GetAutofillManager(GetSenderWebContents());
  if (!autofill_manager)
    return RespondNow(Error(kErrorDataUnavailable));
  autofill::CreditCardAccessManager* credit_card_access_manager =
      autofill_manager->credit_card_access_manager();
  if (!credit_card_access_manager)
    return RespondNow(Error(kErrorDataUnavailable));

  std::unique_ptr<
      api::autofill_private::SetCreditCardFIDOAuthEnabledState::Params>
      parameters = api::autofill_private::SetCreditCardFIDOAuthEnabledState::
          Params::Create(*args_);
  EXTENSION_FUNCTION_VALIDATE(parameters.get());

  credit_card_access_manager->OnSettingsPageFIDOAuthToggled(
      parameters->enabled);
  return RespondNow(NoArguments());
}
}  // namespace extensions
