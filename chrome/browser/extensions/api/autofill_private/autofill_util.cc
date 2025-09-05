// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/autofill_private/autofill_util.h"

#include <stddef.h>

#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/containers/fixed_flat_map.h"
#include "base/containers/to_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/synchronization/lock.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/settings_private/prefs_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/branded_strings.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/country_type.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_profile.h"
#include "components/autofill/core/browser/data_model/addresses/autofill_structured_address_component.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type.h"
#include "components/autofill/core/browser/data_model/autofill_ai/entity_type_names.h"
#include "components/autofill/core/browser/data_model/payments/bnpl_issuer.h"
#include "components/autofill/core/browser/data_model/payments/credit_card.h"
#include "components/autofill/core/browser/data_model/payments/iban.h"
#include "components/autofill/core/browser/field_type_utils.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/payments/constants.h"
#include "components/autofill/core/browser/ui/country_combobox_model.h"
#include "components/autofill/core/common/autofill_payments_features.h"
#include "components/autofill/core/common/autofill_prefs.h"
#include "components/autofill/core/common/credit_card_network_identifiers.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/sync/base/user_selectable_type.h"
#include "components/variations/service/variations_service.h"
#include "extensions/browser/extensions_browser_client.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/webui/web_ui_util.h"
#include "ui/gfx/image/image.h"

namespace autofill_private = extensions::api::autofill_private;

namespace {

bool ShouldUseNewFopDisplay() {
#if BUILDFLAG(IS_ANDROID)
  return false;
#else
  return base::FeatureList::IsEnabled(
      autofill::features::kAutofillEnableNewFopDisplayDesktop);
#endif
}

// Gets the string corresponding to |type| from |profile|.
std::string GetStringFromProfile(const autofill::AutofillProfile& profile,
                                 const autofill::FieldType& type) {
  return base::UTF16ToUTF8(profile.GetRawInfo(type));
}

// Converts AutofillProfile::RecordType enum to the WebUI idl one.
autofill_private::AddressRecordType ConvertProfileRecordType(
    autofill::AutofillProfile::RecordType record_type) {
  switch (record_type) {
    case autofill::AutofillProfile::RecordType::kLocalOrSyncable:
      return autofill_private::AddressRecordType::kLocalOrSyncable;
    case autofill::AutofillProfile::RecordType::kAccount:
      return autofill_private::AddressRecordType::kAccount;
    case autofill::AutofillProfile::RecordType::kAccountHome:
      return autofill_private::AddressRecordType::kAccountHome;
    case autofill::AutofillProfile::RecordType::kAccountWork:
      return autofill_private::AddressRecordType::kAccountWork;
    case autofill::AutofillProfile::RecordType::kAccountNameEmail:
      return autofill_private::AddressRecordType::kAccountNameEmail;
  }
  NOTREACHED();
}

autofill_private::AddressEntry ProfileToAddressEntry(
    const autofill::AutofillProfile& profile,
    const std::u16string& label) {
  autofill_private::AddressEntry address;

  // Add all address fields to the entry.
  address.guid = profile.guid();

  std::ranges::transform(
      autofill::AutofillProfile::kDatabaseStoredTypes,
      back_inserter(address.fields), [&profile](auto field_type) {
        autofill_private::AddressField field;
        field.type =
            autofill_private::ParseFieldType(FieldTypeToStringView(field_type));
        field.value = GetStringFromProfile(profile, field_type);
        return field;
      });

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
  address.metadata->record_type =
      ConvertProfileRecordType(profile.record_type());

  return address;
}

std::string CardNetworkToIconResourceIdString(const std::string& network) {
  if (ShouldUseNewFopDisplay()) {
    static constexpr auto kNetworkToResourceIdStringMap =
        base::MakeFixedFlatMap<std::string_view, std::string_view>(
            {{autofill::kAmericanExpressCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_AMEX"},
             {autofill::kDiscoverCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_DISCOVER"},
             {autofill::kDinersCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_DINERS"},
             {autofill::kEloCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_ELO"},
             {autofill::kJCBCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_JCB"},
             {autofill::kMasterCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_MASTERCARD"},
             {autofill::kMirCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_MIR"},
             {autofill::kTroyCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_TROY"},
             {autofill::kUnionPay,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_UNIONPAY"},
             {autofill::kVerveCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_VERVE"},
             {autofill::kVisaCard,
              "chrome://theme/IDR_AUTOFILL_METADATA_CC_VISA"}});

    auto it = kNetworkToResourceIdStringMap.find(network);
    return it != kNetworkToResourceIdStringMap.end()
               ? std::string(it->second)
               : "chrome://theme/IDR_AUTOFILL_METADATA_CC_GENERIC";
  }
  static constexpr auto kNetworkToResourceIdStringMap =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{autofill::kDiscoverCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_DISCOVER_OLD"},
           {autofill::kMasterCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_MASTERCARD_OLD"},
           {autofill::kVisaCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_VISA_OLD"},
           {autofill::kAmericanExpressCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_AMEX_OLD"},
           {autofill::kDinersCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_DINERS_OLD"},
           {autofill::kJCBCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_JCB_OLD"},
           {autofill::kEloCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_ELO_OLD"},
           {autofill::kMirCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_MIR_OLD"},
           {autofill::kTroyCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_TROY_OLD"},
           {autofill::kUnionPay,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_UNIONPAY_OLD"},
           {autofill::kVerveCard,
            "chrome://theme/IDR_AUTOFILL_METADATA_CC_VERVE_OLD"}});

  auto it = kNetworkToResourceIdStringMap.find(network);
  return it != kNetworkToResourceIdStringMap.end()
             ? std::string(it->second)
             : "chrome://theme/IDR_AUTOFILL_METADATA_CC_GENERIC_OLD";
}

autofill_private::IbanEntry IbanToIbanEntry(const autofill::Iban& iban) {
  autofill_private::IbanEntry iban_entry;

  // Populated IBAN fields need to be converted to an `IbanEntry` to be rendered
  // in the settings page.
  bool is_local = iban.record_type() == autofill::Iban::RecordType::kLocalIban;
  if (is_local) {
    iban_entry.guid = iban.guid();
  } else {
    iban_entry.instrument_id = base::NumberToString(iban.instrument_id());
  }
  if (!iban.nickname().empty()) {
    iban_entry.nickname = base::UTF16ToUTF8(iban.nickname());
  }

  iban_entry.value = base::UTF16ToUTF8(iban.value());

  // Create IBAN metadata and add it to `iban_entry`.
  iban_entry.metadata.emplace();
  iban_entry.metadata->summary_label =
      base::UTF16ToUTF8(iban.GetIdentifierStringForAutofillDisplay());
  iban_entry.metadata->is_local = is_local;

  return iban_entry;
}

std::pair<std::string, std::string> PayOverTimeIssuerToIconResourceIdString(
    autofill::BnplIssuer::IssuerId issuer) {
  switch (issuer) {
    case autofill::BnplIssuer::IssuerId::kBnplAffirm:
      return std::pair<std::string, std::string>(
          "chrome://theme/IDR_AUTOFILL_AFFIRM_LINKED",
          "chrome://theme/IDR_AUTOFILL_AFFIRM_LINKED_DARK");
    case autofill::BnplIssuer::IssuerId::kBnplZip:
      return std::pair<std::string, std::string>(
          "chrome://theme/IDR_AUTOFILL_ZIP_LINKED",
          "chrome://theme/IDR_AUTOFILL_ZIP_LINKED_DARK");
    // TODO(crbug.com/408268581): Handle Afterpay issuer enum value when adding
    // Afterpay to the BNPL flow.
    case autofill::BnplIssuer::IssuerId::kBnplAfterpay:
      return std::pair<std::string, std::string>(
          "chrome://theme/IDR_AUTOFILL_METADATA_BNPL_GENERIC",
          "chrome://theme/IDR_AUTOFILL_METADATA_BNPL_GENERIC");
    case autofill::BnplIssuer::IssuerId::kBnplKlarna:
      return std::pair<std::string, std::string>(
          "chrome://theme/IDR_AUTOFILL_KLARNA_LINKED",
          "chrome://theme/IDR_AUTOFILL_KLARNA_LINKED_DARK");
  }
  NOTREACHED();
}

autofill_private::PayOverTimeIssuerEntry BnplIssuerToPayOverTimeIssuerEntry(
    const autofill::BnplIssuer& issuer) {
  CHECK(issuer.payment_instrument());

  autofill_private::PayOverTimeIssuerEntry issuer_entry;

  issuer_entry.issuer_id =
      autofill::ConvertToBnplIssuerIdString(issuer.issuer_id());
  issuer_entry.instrument_id =
      base::NumberToString(issuer.payment_instrument()->instrument_id());
  issuer_entry.display_name = base::UTF16ToUTF8(issuer.GetDisplayName());

  std::pair<std::string, std::string> issuer_icons =
      PayOverTimeIssuerToIconResourceIdString(issuer.issuer_id());
  issuer_entry.image_src = std::move(issuer_icons.first);
  issuer_entry.image_src_dark = std::move(issuer_icons.second);

  return issuer_entry;
}

}  // namespace

namespace extensions::autofill_util {

AddressEntryList GenerateAddressList(const autofill::AddressDataManager& adm) {
  const std::vector<const autofill::AutofillProfile*> profiles =
      adm.GetProfilesForSettings();

  std::vector<std::u16string> labels =
      autofill::AutofillProfile::CreateDifferentiatingLabels(
          profiles, ExtensionsBrowserClient::Get()->GetApplicationLocale());
  DCHECK_EQ(labels.size(), profiles.size());

  AddressEntryList list;
  list.reserve(profiles.size());
  for (size_t i = 0; i < profiles.size(); ++i) {
    list.push_back(ProfileToAddressEntry(*profiles[i], labels[i]));
  }

  return list;
}

CountryEntryList GenerateCountryList() {
  autofill::CountryComboboxModel model;
  const variations::VariationsService* variations_service =
      g_browser_process->variations_service();
  model.SetCountries(
      autofill::GeoIpCountryCode(variations_service
                                     ? variations_service->GetLatestCountry()
                                     : std::string()),
      extensions::ExtensionsBrowserClient::Get()->GetApplicationLocale());
  const std::vector<std::unique_ptr<autofill::AutofillCountry>>& countries =
      model.countries();

  extensions::autofill_util::CountryEntryList list;
  for (const auto& country : countries) {
    // A null `country` means "insert a space here", so we add a country w/o a
    // `name` or `country_code` to the list and let the UI handle it.
    if (!country) {
      list.emplace_back();
      continue;
    }
    autofill_private::CountryEntry& entry = list.emplace_back();
    entry.name = base::UTF16ToUTF8(country->name());
    entry.country_code = country->country_code();
  }

  return list;
}

CreditCardEntryList GenerateCreditCardList(
    const autofill::PaymentsDataManager& paydm) {
  return base::ToVector(
      paydm.GetCreditCards(), [&paydm](const autofill::CreditCard* card) {
        return CreditCardToCreditCardEntry(*card, paydm,
                                           /*mask_local_cards=*/true);
      });
}

IbanEntryList GenerateIbanList(const autofill::PaymentsDataManager& paydm) {
  return base::ToVector(paydm.GetIbans(), [](const autofill::Iban* iban) {
    return IbanToIbanEntry(*iban);
  });
}

PayOverTimeIssuerEntryList GeneratePayOverTimeIssuerList(
    const autofill::PaymentsDataManager& paydm) {
  std::vector<autofill::BnplIssuer> linked_issuers =
      base::ToVector(paydm.GetLinkedBnplIssuers());

  // Remove the issuer entry if a BNPL issuer is linked externally, due to
  // missing terms of services acceptance.
  linked_issuers.erase(
      std::remove_if(
          linked_issuers.begin(), linked_issuers.end(),
          [](autofill::BnplIssuer& issuer) {
            return issuer.payment_instrument()->action_required().contains(
                autofill::PaymentInstrument::ActionRequired::kAcceptTos);
          }),
      linked_issuers.end());

  return base::ToVector(linked_issuers, &BnplIssuerToPayOverTimeIssuerEntry);
}

std::optional<api::autofill_private::AccountInfo> GetAccountInfo(
    const autofill::AddressDataManager& adm) {
  std::optional<CoreAccountInfo> account = adm.GetPrimaryAccountInfo();
  if (!account.has_value()) {
    return std::nullopt;
  }

  api::autofill_private::AccountInfo api_account;
  api_account.email = account->email;
  api_account.is_sync_enabled_for_autofill_profiles =
      adm.IsSyncFeatureEnabledForAutofill();
  api_account.is_eligible_for_address_account_storage =
      adm.IsEligibleForAddressAccountStorage();
  api_account.is_autofill_sync_toggle_enabled =
      adm.IsAutofillUserSelectableTypeEnabled();
  api_account.is_autofill_sync_toggle_available =
      adm.IsAutofillSyncToggleAvailable();
  return std::move(api_account);
}

autofill_private::CreditCardEntry CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card,
    const autofill::PaymentsDataManager& paydm,
    bool mask_local_cards) {
  autofill_private::CreditCardEntry card;

  // Add all credit card fields to the entry.
  card.guid =
      credit_card.record_type() == autofill::CreditCard::RecordType::kLocalCard
          ? credit_card.guid()
          : credit_card.server_id();
  if (credit_card.record_type() ==
      autofill::CreditCard::RecordType::kMaskedServerCard) {
    card.instrument_id = base::NumberToString(credit_card.instrument_id());
  }
  card.name = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_NAME_FULL));
  std::string full_card_number =
      base::UTF16ToUTF8(credit_card.GetRawInfo(autofill::CREDIT_CARD_NUMBER));
  card.card_number =
      (credit_card.record_type() ==
           autofill::CreditCard::RecordType::kLocalCard &&
       full_card_number.length() > 4 && mask_local_cards)
          ? full_card_number.substr(full_card_number.length() - 4)
          : full_card_number;
  card.expiration_month = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_MONTH));
  card.expiration_year = base::UTF16ToUTF8(
      credit_card.GetRawInfo(autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR));
  card.network = base::UTF16ToUTF8(credit_card.NetworkForDisplay());
  if (!credit_card.nickname().empty()) {
    card.nickname = base::UTF16ToUTF8(credit_card.nickname());
  }
  const gfx::Image* card_art_image =
      paydm.GetCachedCardArtImageForUrl(credit_card.card_art_url());
  card.image_src =
      card_art_image ? webui::GetBitmapDataUrl(card_art_image->AsBitmap())
                     : CardNetworkToIconResourceIdString(credit_card.network());
  if (paydm.IsCardEligibleForBenefits(credit_card) &&
      credit_card.product_terms_url().is_valid()) {
    card.product_terms_url = credit_card.product_terms_url().spec();
  }

  // Create card metadata and add it to |card|.
  card.metadata.emplace();
  std::pair<std::u16string, std::u16string> label_pieces =
      credit_card.LabelPieces();
  card.metadata->summary_label = base::UTF16ToUTF8(label_pieces.first);
  card.metadata->summary_sublabel = base::UTF16ToUTF8(label_pieces.second);
  card.metadata->is_local =
      credit_card.record_type() == autofill::CreditCard::RecordType::kLocalCard;
  // IsValid() checks if both card number and expiration date are valid.
  // IsServerCard() checks whether there is a duplicated server card in
  // `paydm`.
  card.metadata->is_migratable =
      credit_card.IsValid() && !paydm.IsServerCard(&credit_card);
  card.metadata->is_virtual_card_enrollment_eligible =
      credit_card.virtual_card_enrollment_state() ==
          autofill::CreditCard::VirtualCardEnrollmentState::kEnrolled ||
      credit_card.virtual_card_enrollment_state() ==
          autofill::CreditCard::VirtualCardEnrollmentState::
              kUnenrolledAndEligible;
  card.metadata->is_virtual_card_enrolled =
      credit_card.virtual_card_enrollment_state() ==
      autofill::CreditCard::VirtualCardEnrollmentState::kEnrolled;

  if (!credit_card.cvc().empty()) {
    // Replace all the chars in the CVC with "•" for security when
    // the `credit_card` type is a `kMaskedServerCard` or `mask_local_cards` is
    // true.
    card.cvc = base::UTF16ToUTF8(credit_card.cvc());
    if (credit_card.record_type() ==
            autofill::CreditCard::RecordType::kMaskedServerCard ||
        mask_local_cards) {
      card.cvc = base::UTF16ToUTF8(std::u16string(card.cvc->size(), u'•'));
    }
  }

  return card;
}

}  // namespace extensions::autofill_util
