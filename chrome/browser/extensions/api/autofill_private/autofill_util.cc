// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"

#include <stddef.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/strings/string_split.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "chrome/common/pref_names.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"

namespace autofill_private = extensions::api::autofill_private;

namespace {

// Get the multi-valued element for |type| and return it as a |vector|.
// TODO(khorimoto): remove this function since multi-valued types are
// deprecated.
std::unique_ptr<std::vector<std::string>> GetValueList(
    const autofill::AutofillProfile& profile,
    autofill::ServerFieldType type) {
  std::unique_ptr<std::vector<std::string>> list(new std::vector<std::string>);

  std::vector<std::u16string> values;
  if (autofill::AutofillType(type).group() == autofill::FieldTypeGroup::kName) {
    values.push_back(
        profile.GetInfo(autofill::AutofillType(type),
                        g_browser_process->GetApplicationLocale()));
  } else {
    values.push_back(profile.GetRawInfo(type));
  }

  // |Get[Raw]MultiInfo()| always returns at least one, potentially empty, item.
  // If this is the case, there is no info to return, so return an empty vector.
  if (values.size() == 1 && values.front().empty())
    return list;

  for (const std::u16string& value16 : values)
    list->push_back(base::UTF16ToUTF8(value16));

  return list;
}

// Gets the string corresponding to |type| from |profile|.
std::unique_ptr<std::string> GetStringFromProfile(
    const autofill::AutofillProfile& profile,
    const autofill::ServerFieldType& type) {
  return std::make_unique<std::string>(
      base::UTF16ToUTF8(profile.GetRawInfo(type)));
}

autofill_private::AddressEntry ProfileToAddressEntry(
    const autofill::AutofillProfile& profile,
    const std::u16string& label) {
  autofill_private::AddressEntry address;

  // Add all address fields to the entry.
  address.guid = std::make_unique<std::string>(profile.guid());
  address.full_names = GetValueList(profile, autofill::NAME_FULL);
  address.honorific =
      GetStringFromProfile(profile, autofill::NAME_HONORIFIC_PREFIX);
  address.company_name = GetStringFromProfile(profile, autofill::COMPANY_NAME);
  address.address_lines =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_STREET_ADDRESS);
  address.address_level1 =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_STATE);
  address.address_level2 =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_CITY);
  address.address_level3 =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_DEPENDENT_LOCALITY);
  address.postal_code =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_ZIP);
  address.sorting_code =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_SORTING_CODE);
  address.country_code =
      GetStringFromProfile(profile, autofill::ADDRESS_HOME_COUNTRY);
  address.phone_numbers =
      GetValueList(profile, autofill::PHONE_HOME_WHOLE_NUMBER);
  address.email_addresses = GetValueList(profile, autofill::EMAIL_ADDRESS);
  address.language_code =
      std::make_unique<std::string>(profile.language_code());

  // Parse |label| so that it can be used to create address metadata.
  std::u16string separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
  std::vector<std::u16string> label_pieces = base::SplitStringUsingSubstr(
      label, separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Create address metadata and add it to |address|.
  std::unique_ptr<autofill_private::AutofillMetadata> metadata(
      new autofill_private::AutofillMetadata);
  metadata->summary_label = base::UTF16ToUTF8(label_pieces[0]);
  metadata->summary_sublabel = std::make_unique<std::string>(
      base::UTF16ToUTF8(label.substr(label_pieces[0].size())));
  address.metadata = std::move(metadata);

  return address;
}

autofill_private::CountryEntry CountryToCountryEntry(
    autofill::AutofillCountry* country) {
  autofill_private::CountryEntry entry;

  // A null |country| means "insert a space here", so we add a country w/o a
  // |name| or |country_code| to the list and let the UI handle it.
  if (country) {
    entry.name =
        std::make_unique<std::string>(base::UTF16ToUTF8(country->name()));
    entry.country_code = std::make_unique<std::string>(country->country_code());
  }

  return entry;
}

autofill_private::CreditCardEntry CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card,
    const autofill::PersonalDataManager& personal_data) {
  autofill_private::CreditCardEntry card;

  // Add all credit card fields to the entry.
  card.guid = std::make_unique<std::string>(credit_card.guid());
  card.name = std::make_unique<std::string>(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL)));
  card.card_number = std::make_unique<std::string>(
      base::UTF16ToUTF8(credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER)));
  card.expiration_month = std::make_unique<std::string>(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH)));
  card.expiration_year = std::make_unique<std::string>(base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR)));
  if (!credit_card.nickname().empty()) {
    card.nickname = std::make_unique<std::string>(
        base::UTF16ToUTF8(credit_card.nickname()));
  }

  // Create card metadata and add it to |card|.
  std::unique_ptr<autofill_private::AutofillMetadata> metadata(
      new autofill_private::AutofillMetadata);
  std::pair<std::u16string, std::u16string> label_pieces =
      credit_card.LabelPieces();
  metadata->summary_label = base::UTF16ToUTF8(label_pieces.first);
  metadata->summary_sublabel =
      std::make_unique<std::string>(base::UTF16ToUTF8(label_pieces.second));
  metadata->is_local = std::make_unique<bool>(credit_card.record_type() ==
                                              autofill::CreditCard::LOCAL_CARD);
  metadata->is_cached = std::make_unique<bool>(
      credit_card.record_type() == autofill::CreditCard::FULL_SERVER_CARD);
  // IsValid() checks if both card number and expiration date are valid.
  // IsServerCard() checks whether there is a duplicated server card in
  // |personal_data|.
  metadata->is_migratable = std::make_unique<bool>(
      credit_card.IsValid() && !personal_data.IsServerCard(&credit_card));
  card.metadata = std::move(metadata);

  return card;
}

}  // namespace

namespace extensions {

namespace autofill_util {

AddressEntryList GenerateAddressList(
    const autofill::PersonalDataManager& personal_data) {
  const std::vector<autofill::AutofillProfile*>& profiles =
      personal_data.GetProfiles();
  std::vector<std::u16string> labels;
  autofill::AutofillProfile::CreateDifferentiatingLabels(
      profiles, g_browser_process->GetApplicationLocale(), &labels);
  DCHECK_EQ(labels.size(), profiles.size());

  AddressEntryList list;
  for (size_t i = 0; i < profiles.size(); ++i)
    list.push_back(ProfileToAddressEntry(*profiles[i], labels[i]));

  return list;
}

CountryEntryList GenerateCountryList(
    const autofill::PersonalDataManager& personal_data) {
  autofill::CountryComboboxModel model;
  model.SetCountries(personal_data,
                     base::RepeatingCallback<bool(const std::string&)>(),
                     g_browser_process->GetApplicationLocale());
  const std::vector<std::unique_ptr<autofill::AutofillCountry>>& countries =
      model.countries();

  CountryEntryList list;

  for (const auto& country : countries)
    list.push_back(CountryToCountryEntry(country.get()));

  return list;
}

CreditCardEntryList GenerateCreditCardList(
    const autofill::PersonalDataManager& personal_data) {
  const std::vector<autofill::CreditCard*>& cards =
      personal_data.GetCreditCards();

  CreditCardEntryList list;
  for (const autofill::CreditCard* card : cards)
    list.push_back(CreditCardToCreditCardEntry(*card, personal_data));

  return list;
}

}  // namespace autofill_util

}  // namespace extensions
