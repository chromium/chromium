// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_H_

#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"

// Filters data from the personal data manager for Fast Checkout's purposes,
// i.e. returns valid and complete profiles and credit cards.
class FastCheckoutPersonalDataHelper {
 public:
  virtual ~FastCheckoutPersonalDataHelper() = default;

  FastCheckoutPersonalDataHelper(const FastCheckoutPersonalDataHelper&) =
      delete;
  FastCheckoutPersonalDataHelper& operator=(
      const FastCheckoutPersonalDataHelper&) = delete;

  // Returns profiles to suggest.
  virtual std::vector<const autofill::AutofillProfile*> GetProfilesToSuggest()
      const = 0;

  // Returns credit cards to suggest that have a number.
  virtual std::vector<autofill::CreditCard*> GetCreditCardsToSuggest()
      const = 0;

  // Returns unexpired credit cards with valid number and name.
  virtual std::vector<autofill::CreditCard*> GetValidCreditCards() const = 0;

  // Returns profiles with name, address, country, email and phone number.
  virtual std::vector<const autofill::AutofillProfile*>
  GetValidAddressProfiles() const = 0;

  // Returns the current profile's `PersonalDataManager` instance.
  virtual autofill::PersonalDataManager* GetPersonalDataManager() const = 0;

 protected:
  FastCheckoutPersonalDataHelper() = default;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_H_
