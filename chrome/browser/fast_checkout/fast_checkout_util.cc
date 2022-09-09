// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_util.h"

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/geo/autofill_country.h"

namespace fast_checkout {

autofill_assistant::external::ProfileProto CreateProfileProto(
    const autofill::AutofillProfile& autofill_profile) {
  autofill_assistant::external::ProfileProto profile_proto;
  const std::string locale = g_browser_process->GetApplicationLocale();

  autofill::ServerFieldTypeSet types;
  autofill_profile.GetNonEmptyTypes(locale, &types);

  for (autofill::ServerFieldType type : types) {
    if (autofill_profile.HasInfo(type)) {
      (*profile_proto.mutable_values())[type] =
          base::UTF16ToUTF8(autofill_profile.GetInfo(type, locale));
    }
  }

  return profile_proto;
}

autofill_assistant::external::CreditCardProto CreateCreditCardProto(
    const autofill::CreditCard& credit_card) {
  autofill_assistant::external::CreditCardProto card_proto;
  const std::string locale = g_browser_process->GetApplicationLocale();

  autofill::ServerFieldTypeSet types;
  credit_card.GetNonEmptyTypes(locale, &types);

  for (autofill::ServerFieldType type : types) {
    if (credit_card.HasInfo(type)) {
      (*card_proto.mutable_values())[type] =
          base::UTF16ToUTF8(credit_card.GetInfo(type, locale));
    }
  }

  card_proto.set_record_type(credit_card.record_type());
  card_proto.set_instrument_id(credit_card.instrument_id());

  if (!credit_card.network().empty()) {
    card_proto.set_network(credit_card.network());
  }

  if (!credit_card.server_id().empty()) {
    card_proto.set_server_id(credit_card.server_id());
  }

  return card_proto;
}

bool IsCompleteAddressProfile(const autofill::AutofillProfile* profile,
                              const std::string& app_locale) {
  std::string country_code =
      base::UTF16ToASCII(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  if (country_code.empty()) {
    return false;
  }

  autofill::AutofillCountry country(country_code, app_locale);
  return !profile->GetInfo(autofill::NAME_FULL, app_locale).empty() &&
         !profile->GetRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS).empty() &&
         (!country.requires_zip() ||
          profile->HasRawInfo(autofill::ADDRESS_HOME_ZIP)) &&
         !profile->GetRawInfo(autofill::EMAIL_ADDRESS).empty() &&
         !profile->GetRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER).empty();
}

}  // namespace fast_checkout
