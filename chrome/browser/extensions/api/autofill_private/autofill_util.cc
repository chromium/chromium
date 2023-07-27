// Copyright 2015 The Chromium Authors
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
#include "chrome/grit/chromium_strings.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/data_model/iban.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

namespace autofill_private = extensions::api::autofill_private;

namespace {

// Get the multi-valued element for |type| and return it as a |vector|.
// TODO(khorimoto): remove this function since multi-valued types are
// deprecated.
std::vector<std::string> GetList(const autofill::AutofillProfile& profile,
                                 autofill::ServerFieldType type) {
  std::vector<std::string> list;

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
    list.push_back(base::UTF16ToUTF8(value16));

  return list;
}

// Gets the string corresponding to |type| from |profile|.
std::string GetStringFromProfile(const autofill::AutofillProfile& profile,
                                 const autofill::ServerFieldType& type) {
  return base::UTF16ToUTF8(profile.GetRawInfo(type));
}

// Converts AutofillProfile::Source enum to the WebUI idl one.
autofill_private::AddressSource ConvertProfileSource(
    autofill::AutofillProfile::Source source) {
  switch (source) {
    case autofill::AutofillProfile::Source::kLocalOrSyncable:
      return autofill_private::AddressSource::kLocalOrSyncable;
    case autofill::AutofillProfile::Source::kAccount:
      return autofill_private::AddressSource::kAccount;
    default:
      NOTREACHED();
      return autofill_private::AddressSource::kNone;
  }
}

autofill_private::AddressEntry ProfileToAddressEntry(
    const autofill::AutofillProfile& profile,
    const std::u16string& label) {
  autofill_private::AddressEntry address;

  // Add all address fields to the entry.
  address.guid = profile.guid();
  address.full_name = GetStringFromProfile(profile, autofill::NAME_FULL);
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
  address.phone_number =
      GetStringFromProfile(profile, autofill::PHONE_HOME_WHOLE_NUMBER);
  address.email_address =
      GetStringFromProfile(profile, autofill::EMAIL_ADDRESS);
  address.language_code = profile.language_code();

  // Parse |label| so that it can be used to create address metadata.
  std::u16string separator =
      l10n_util::GetStringUTF16(IDS_AUTOFILL_ADDRESS_SUMMARY_SEPARATOR);
  std::vector<std::u16string> label_pieces = base::SplitStringUsingSubstr(
      label, separator, base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);

  // Create address metadata and add it to |address|.
  address.metadata.emplace();
  address.metadata->summary_label = base::UTF16ToUTF8(label_pieces[0]);
  address.metadata->summary_sublabel =
      base::UTF16ToUTF8(label.substr(label_pieces[0].size()));
  address.metadata->source = ConvertProfileSource(profile.source());

  return address;
}

autofill_private::CountryEntry CountryToCountryEntry(
    autofill::AutofillCountry* country) {
  autofill_private::CountryEntry entry;

  // A null |country| means "insert a space here", so we add a country w/o a
  // |name| or |country_code| to the list and let the UI handle it.
  if (country) {
    entry.name = base::UTF16ToUTF8(country->name());
    entry.country_code = country->country_code();
  }

  return entry;
}

std::string CardNetworkToIconResourceIdString(const std::string& network) {
  bool metadata_icon = base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableNewCardArtAndNetworkImages);

  if (network == autofill::kAmericanExpressCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_AMEX"
                         : "chrome://theme/IDR_AUTOFILL_CC_AMEX";
  }
  if (network == autofill::kDinersCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_DINERS"
                         : "chrome://theme/IDR_AUTOFILL_CC_DINERS";
  }
  if (network == autofill::kDiscoverCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_DISCOVER"
                         : "chrome://theme/IDR_AUTOFILL_CC_DISCOVER";
  }
  if (network == autofill::kEloCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_ELO"
                         : "chrome://theme/IDR_AUTOFILL_CC_ELO";
  }
  if (network == autofill::kJCBCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_JCB"
                         : "chrome://theme/IDR_AUTOFILL_CC_JCB";
  }
  if (network == autofill::kMasterCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_MASTERCARD"
                         : "chrome://theme/IDR_AUTOFILL_CC_MASTERCARD";
  }
  if (network == autofill::kMirCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_MIR"
                         : "chrome://theme/IDR_AUTOFILL_CC_MIR";
  }
  if (network == autofill::kTroyCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_TROY"
                         : "chrome://theme/IDR_AUTOFILL_CC_TROY";
  }
  if (network == autofill::kUnionPay) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_UNIONPAY"
                         : "chrome://theme/IDR_AUTOFILL_CC_UNIONPAY";
  }
  if (network == autofill::kVisaCard) {
    return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_VISA"
                         : "chrome://theme/IDR_AUTOFILL_CC_VISA";
  }

  return metadata_icon ? "chrome://theme/IDR_AUTOFILL_METADATA_CC_GENERIC"
                       : "chrome://theme/IDR_AUTOFILL_CC_GENERIC";
}

autofill_private::CreditCardEntry CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card,
    const autofill::PersonalDataManager& personal_data) {
  autofill_private::CreditCardEntry card;

  // Add all credit card fields to the entry.
  card.guid = credit_card.record_type() == autofill::CreditCard::LOCAL_CARD
                  ? credit_card.guid()
                  : credit_card.server_id();
  card.name = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  card.card_number =
      base::UTF16ToUTF8(credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER));
  card.expiration_month = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH));
  card.expiration_year = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));
  card.network = base::UTF16ToUTF8(credit_card.NetworkForDisplay());
  if (!credit_card.nickname().empty()) {
    card.nickname = base::UTF16ToUTF8(credit_card.nickname());
  }
  gfx::Image* card_art_image = nullptr;
  if (base::FeatureList::IsEnabled(
          autofill::features::kAutofillEnableCardArtImage)) {
    card_art_image =
        personal_data.GetCreditCardArtImageForUrl(credit_card.card_art_url());
  }
  card.image_src =
      card_art_image ? webui::GetBitmapDataUrl(card_art_image->AsBitmap())
                     : CardNetworkToIconResourceIdString(credit_card.network());

  // Create card metadata and add it to |card|.
  card.metadata.emplace();
  std::pair<std::u16string, std::u16string> label_pieces =
      credit_card.LabelPieces();
  card.metadata->summary_label = base::UTF16ToUTF8(label_pieces.first);
  card.metadata->summary_sublabel = base::UTF16ToUTF8(label_pieces.second);
  card.metadata->is_local =
      credit_card.record_type() == autofill::CreditCard::LOCAL_CARD;
  card.metadata->is_cached =
      credit_card.record_type() == autofill::CreditCard::FULL_SERVER_CARD;
  // IsValid() checks if both card number and expiration date are valid.
  // IsServerCard() checks whether there is a duplicated server card in
  // |personal_data|.
  card.metadata->is_migratable =
      credit_card.IsValid() && !personal_data.IsServerCard(&credit_card);
  card.metadata->is_virtual_card_enrollment_eligible =
      credit_card.virtual_card_enrollment_state() ==
          autofill::CreditCard::VirtualCardEnrollmentState::kEnrolled ||
      credit_card.virtual_card_enrollment_state() ==
          autofill::CreditCard::VirtualCardEnrollmentState::
              kUnenrolledAndEligible;
  card.metadata->is_virtual_card_enrolled =
      credit_card.virtual_card_enrollment_state() ==
      autofill::CreditCard::VirtualCardEnrollmentState::kEnrolled;

  return card;
}

autofill_private::IbanEntry IbanToIbanEntry(
    const autofill::IBAN& iban,
    const autofill::PersonalDataManager& personal_data) {
  autofill_private::IbanEntry iban_entry;

  // Populated IBAN fields need to be converted to an `IbanEntry` to be rendered
  // in the settings page.
  iban_entry.guid = iban.guid();
  if (!iban.nickname().empty())
    iban_entry.nickname = base::UTF16ToUTF8(iban.nickname());

  iban_entry.value = base::UTF16ToUTF8(iban.value());

  // Create IBAN metadata and add it to `iban_entry`.
  iban_entry.metadata.emplace();
  iban_entry.metadata->summary_label =
      base::UTF16ToUTF8(iban.GetIdentifierStringForAutofillDisplay());

  return iban_entry;
}

}  // namespace

namespace extensions::autofill_util {

AddressEntryList GenerateAddressList(
    const autofill::PersonalDataManager& personal_data) {
  const std::vector<autofill::AutofillProfile*>& profiles =
      personal_data.GetProfilesForSettings();
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

IbanEntryList GenerateIbanList(
    const autofill::PersonalDataManager& personal_data) {
  IbanEntryList list;
  for (const autofill::IBAN* iban : personal_data.GetLocalIBANs())
    list.push_back(IbanToIbanEntry(*iban, personal_data));

  return list;
}

absl::optional<api::autofill_private::AccountInfo> GetAccountInfo(
    const autofill::PersonalDataManager& personal_data) {
  absl::optional<CoreAccountInfo> account =
      personal_data.GetPrimaryAccountInfo();
  if (!account.has_value()) {
    return absl::nullopt;
  }

  api::autofill_private::AccountInfo api_account;
  api_account.email = account->email;
  api_account.is_sync_enabled_for_autofill_profiles =
      personal_data.IsSyncEnabledFor(syncer::UserSelectableType::kAutofill);
  api_account.is_eligible_for_address_account_storage =
      personal_data.IsEligibleForAddressAccountStorage();
  return std::move(api_account);
}

void AuthenticateUser(
    scoped_refptr<device_reauth::DeviceAuthenticator> device_authenticator,
    const std::u16string& prompt_message,
    CallbackAfterSuccessfulUserAuth callback) {
#if BUILDFLAG(IS_MAC) || BUILDFLAG(IS_WIN)
  CHECK(device_authenticator);
  device_authenticator->AuthenticateWithMessage(prompt_message,
                                                std::move(callback));
#endif
}

}  // namespace extensions::autofill_util
