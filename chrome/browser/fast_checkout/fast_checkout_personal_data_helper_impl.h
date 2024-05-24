// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_IMPL_H_
#define CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_IMPL_H_

#include "chrome/browser/fast_checkout/fast_checkout_personal_data_helper.h"
#include "components/autofill/core/browser/data_model/autofill_profile.h"
#include "components/autofill/core/browser/data_model/credit_card.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "content/public/browser/web_contents.h"

class FastCheckoutPersonalDataHelperImpl
    : public FastCheckoutPersonalDataHelper {
 public:
  explicit FastCheckoutPersonalDataHelperImpl(
      content::WebContents* web_contents);
  ~FastCheckoutPersonalDataHelperImpl() override = default;

  FastCheckoutPersonalDataHelperImpl(
      const FastCheckoutPersonalDataHelperImpl&) = delete;
  FastCheckoutPersonalDataHelperImpl& operator=(
      const FastCheckoutPersonalDataHelperImpl&) = delete;

  // FastCheckoutPersonalDataHelper:
  std::vector<const autofill::AutofillProfile*> GetProfilesToSuggest()
      const override;
  std::vector<autofill::CreditCard*> GetCreditCardsToSuggest() const override;
  std::vector<autofill::CreditCard*> GetValidCreditCards() const override;
  std::vector<const autofill::AutofillProfile*> GetValidAddressProfiles()
      const override;
  autofill::PersonalDataManager* GetPersonalDataManager() const override;

 private:
  bool IsCompleteAddressProfile(const autofill::AutofillProfile* profile,
                                const std::string& app_locale) const;

  raw_ptr<content::WebContents> web_contents_;
};

#endif  // CHROME_BROWSER_FAST_CHECKOUT_FAST_CHECKOUT_PERSONAL_DATA_HELPER_IMPL_H_
