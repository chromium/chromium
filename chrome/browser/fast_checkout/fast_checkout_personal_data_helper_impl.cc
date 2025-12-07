// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"

#include <functional>
#include <vector>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "components/autofill/core/browser/data_manager/addresses/address_data_manager.h"
#include "components/autofill/core/browser/data_manager/payments/payments_data_manager.h"
#include "components/autofill/core/browser/geo/autofill_country.h"
#include "components/autofill/core/browser/suggestions/payments/payments_suggestion_generator.h"

FastCheckoutPersonalDataHelperImpl::FastCheckoutPersonalDataHelperImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

autofill::PersonalDataManager*
FastCheckoutPersonalDataHelperImpl::GetPersonalDataManager() const {
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForBrowserContext(
          web_contents_->GetBrowserContext());
  DCHECK(pdm);
  return pdm;
}

std::vector<const autofill::AutofillProfile*>
FastCheckoutPersonalDataHelperImpl::GetProfilesToSuggest() const {
  return GetPersonalDataManager()
      ->address_data_manager()
      .GetProfilesToSuggest();
}

std::vector<const autofill::CreditCard*>
FastCheckoutPersonalDataHelperImpl::GetCreditCardsToSuggest() const {
  std::vector<const autofill::CreditCard*> cards_to_suggest =
      autofill::GetCreditCardsToSuggest(
          GetPersonalDataManager()->payments_data_manager());
  // Do not offer cards with empty number.
  std::erase_if(cards_to_suggest, [](const autofill::CreditCard* card) {
    return !card->HasRawInfo(autofill::CREDIT_CARD_NUMBER);
  });
  return cards_to_suggest;
}

bool FastCheckoutPersonalDataHelperImpl::IsCompleteAddressProfile(
    const autofill::AutofillProfile* profile,
    const std::string& app_locale) const {
  if (!profile->HasRawInfo(autofill::ADDRESS_HOME_COUNTRY)) {
    return false;
  }

  std::string country_code =
      base::UTF16ToASCII(profile->GetRawInfo(autofill::ADDRESS_HOME_COUNTRY));
  autofill::AutofillCountry country(country_code, app_locale);
  return profile->HasRawInfo(autofill::NAME_FULL) &&
         profile->HasRawInfo(autofill::ADDRESS_HOME_STREET_ADDRESS) &&
         (!country.requires_zip() ||
          profile->HasRawInfo(autofill::ADDRESS_HOME_ZIP)) &&
         profile->HasRawInfo(autofill::EMAIL_ADDRESS) &&
         profile->HasRawInfo(autofill::PHONE_HOME_WHOLE_NUMBER);
}

std::vector<const autofill::CreditCard*>
FastCheckoutPersonalDataHelperImpl::GetValidCreditCards() const {
  std::vector<const autofill::CreditCard*> cards =
      autofill::GetCreditCardsToSuggest(
          GetPersonalDataManager()->payments_data_manager());
  std::erase_if(cards, std::not_fn(&autofill::CreditCard::IsCompleteValidCard));
  return cards;
}

std::vector<const autofill::AutofillProfile*>
FastCheckoutPersonalDataHelperImpl::GetValidAddressProfiles() const {
  const autofill::AddressDataManager& adm =
      GetPersonalDataManager()->address_data_manager();
  // Trigger only if there is at least 1 complete address profile on file.
  std::vector<const autofill::AutofillProfile*> profiles =
      adm.GetProfilesToSuggest();

  std::erase_if(profiles,
                [&adm, this](const autofill::AutofillProfile* profile) {
                  return !IsCompleteAddressProfile(profile, adm.app_locale());
                });
  return profiles;
}
