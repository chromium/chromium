// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper_impl.h"

#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "components/autofill/core/browser/geo/autofill_country.h"

FastCheckoutPersonalDataHelperImpl::FastCheckoutPersonalDataHelperImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

autofill::PersonalDataManager*
FastCheckoutPersonalDataHelperImpl::GetPersonalDataManager() const {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  autofill::PersonalDataManager* pdm =
      autofill::PersonalDataManagerFactory::GetForProfile(
          profile->GetOriginalProfile());
  DCHECK(pdm);
  return pdm;
}

std::vector<autofill::AutofillProfile*>
FastCheckoutPersonalDataHelperImpl::GetProfilesToSuggest() const {
  return GetPersonalDataManager()->GetProfilesToSuggest();
}

std::vector<autofill::CreditCard*>
FastCheckoutPersonalDataHelperImpl::GetCreditCardsToSuggest() const {
  std::vector<autofill::CreditCard*> cards_to_suggest =
      GetPersonalDataManager()->GetCreditCardsToSuggest();
  // Do not offer cards with empty number.
  base::EraseIf(cards_to_suggest, [](const autofill::CreditCard* card) {
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

std::vector<autofill::CreditCard*>
FastCheckoutPersonalDataHelperImpl::GetValidCreditCards() const {
  std::vector<autofill::CreditCard*> cards =
      GetPersonalDataManager()->GetCreditCardsToSuggest();
  base::EraseIf(cards,
                base::not_fn(&autofill::CreditCard::IsCompleteValidCard));
  return cards;
}

std::vector<autofill::AutofillProfile*>
FastCheckoutPersonalDataHelperImpl::GetValidAddressProfiles() const {
  autofill::PersonalDataManager* pdm = GetPersonalDataManager();
  // Trigger only if there is at least 1 complete address profile on file.
  std::vector<autofill::AutofillProfile*> profiles =
      pdm->GetProfilesToSuggest();

  base::EraseIf(profiles,
                [&pdm, this](const autofill::AutofillProfile* profile) {
                  return !IsCompleteAddressProfile(profile, pdm->app_locale());
                });
  return profiles;
}
