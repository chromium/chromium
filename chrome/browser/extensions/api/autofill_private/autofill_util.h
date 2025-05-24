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
#include "components/device_reauth/device_authenticator.h"

namespace autofill {
class AddressDataManager;
class CreditCard;
class PaymentsDataManager;
}  // namespace autofill

namespace extensions::autofill_util {

using AddressEntryList = std::vector<api::autofill_private::AddressEntry>;
using CountryEntryList = std::vector<api::autofill_private::CountryEntry>;
using CreditCardEntryList = std::vector<api::autofill_private::CreditCardEntry>;
using IbanEntryList = std::vector<api::autofill_private::IbanEntry>;
using PayOverTimeIssuerEntryList =
    std::vector<api::autofill_private::PayOverTimeIssuerEntry>;
using CallbackAfterSuccessfulUserAuth = base::OnceCallback<void(bool)>;

// Uses `adm` to generate a list of up-to-date AddressEntry objects.
AddressEntryList GenerateAddressList(const autofill::AddressDataManager& adm);

// Generate a list of up-to-date `CountryEntry` objects that can be stored.
CountryEntryList GenerateCountryList();

// Uses `paydm` to generate a list of up-to-date CreditCardEntry
// objects.
CreditCardEntryList GenerateCreditCardList(
    const autofill::PaymentsDataManager& paydm);

// Uses `paydm` to generate a list of up-to-date IbanEntry
// objects.
IbanEntryList GenerateIbanList(const autofill::PaymentsDataManager& paydm);

// Uses `paydm` to generate a list of up-to-date PayOverTimeIssuerEntry
// objects.
PayOverTimeIssuerEntryList GeneratePayOverTimeIssuerList(
    const autofill::PaymentsDataManager& paydm);

// Uses `adm` to get primary account info.
std::optional<api::autofill_private::AccountInfo> GetAccountInfo(
    const autofill::AddressDataManager& adm);

// Returns a `CreditCardEntry` object which is UI compatible.
api::autofill_private::CreditCardEntry CreditCardToCreditCardEntry(
    const autofill::CreditCard& credit_card,
    const autofill::PaymentsDataManager& paydm,
    bool mask_local_cards);

}  // namespace extensions::autofill_util

#endif  // CHROME_BROWSER_EXTENSIONS_API_AUTOFILL_PRIVATE_AUTOFILL_UTIL_H_
