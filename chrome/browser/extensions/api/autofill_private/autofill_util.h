// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
#define CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_

#include <map>
#include <memory>
#include <optional>

#include "base/functional/callback_forward.h"
#include "chrome/common/extensions/api/autofill_private.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/device_reauth/device_authenticator.h"

namespace extensions {

namespace autofill_util {

using AddressEntryList = std::vector<api::autofill_private::AddressEntry>;
using CountryEntryList = std::vector<api::autofill_private::CountryEntry>;
using CreditCardEntryList = std::vector<api::autofill_private::CreditCardEntry>;
using IbanEntryList = std::vector<api::autofill_private::IbanEntry>;
using CallbackAfterSuccessfulUserAuth = base::OnceCallback<void(bool)>;

// Uses |personal_data| to generate a list of up-to-date AddressEntry objects.
AddressEntryList GenerateAddressList(
    const autofill::PersonalDataManager& personal_data);

// Uses `personal_data` to generate a list of up-to-date CountryEntry objects.
// Depending on the `for_account_address_profile` and
// `AutofillEnableAccountStorageForIneligibleCountries`, unsupported countries
// are filtered from the resulting list.
CountryEntryList GenerateCountryList(
    const autofill::PersonalDataManager& personal_data,
    bool for_account_address_profile);

// Uses |personal_data| to generate a list of up-to-date CreditCardEntry
// objects.
CreditCardEntryList GenerateCreditCardList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to generate a list of up-to-date IbanEntry
// objects.
IbanEntryList GenerateIbanList(
    const autofill::PersonalDataManager& personal_data);

// Uses |personal_data| to get primary account info.
std::optional<api::autofill_private::AccountInfo> GetAccountInfo(
    const autofill::PersonalDataManager& personal_data);

// Returns a `CreditCardEntry` object which is UI compatible.
api::autofill_private::CreditCardEntry CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card,
    const autofill::PersonalDataManager& personal_data,
    bool mask_local_cards);

}  // namespace autofill_util

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
